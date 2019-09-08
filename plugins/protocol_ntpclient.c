/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Very lightweight minimal ntp client
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>

#define LWSNTPC_LI_NONE			0
#define LWSNTPC_VN_3			3
#define LWSNTPC_MODE_CLIENT		3

typedef enum {
	LWSNTPC_IDLE,
	LWSNTPC_SENDING,
	LWSNTPC_WAITING,
} lws_ntpc_state_t;

struct vhd_ntpc {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
	lws_sorted_usec_list_t sul;
	const char *ntp_server_ads;
	struct lws *wsi_udp;

	lws_ntpc_state_t state;
};

static void
lws_ntpc_retry(struct lws_sorted_usec_list *sul)
{
	struct vhd_ntpc *v = lws_container_of(sul, struct vhd_ntpc, sul);

	if (v->wsi_udp)
		return;

	/* create the UDP socket aimed at the server */

	lwsl_notice("%s: server %s\n", __func__, v->ntp_server_ads);
	v->wsi_udp = lws_create_adopt_udp(v->vhost, v->ntp_server_ads, 123, 0,
					  v->protocol->name, NULL);
	if (!v->wsi_udp) {
		lwsl_err("%s: unable to create udp skt\n", __func__);
		lws_sul_schedule(v->context, 0, &v->sul, lws_ntpc_retry,
				 5 * LWS_US_PER_SEC);
	}
}

static int
callback_ntpc(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct vhd_ntpc *v = (struct vhd_ntpc *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
						 lws_get_protocol(wsi));
	uint8_t pkt[LWS_PRE + 48];

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT: /* per vhost */
		if (v)
			break;

		lwsl_notice("LWS_CALLBACK_PROTOCOL_INIT\n");
		lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
					    lws_get_protocol(wsi), sizeof(*v));
		v = (struct vhd_ntpc *)lws_protocol_vh_priv_get(lws_get_vhost(wsi),
							   lws_get_protocol(wsi));
		v->context = lws_get_context(wsi);
		v->vhost = lws_get_vhost(wsi);
		v->protocol = lws_get_protocol(wsi);
		if (lws_pvo_get_str(in, "server", &v->ntp_server_ads))
			return 1;

		lws_sul_schedule(v->context, 0, &v->sul, lws_ntpc_retry, 50);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY: /* per vhost */
		if (!v)
			break;
		if (v->wsi_udp)
			lws_set_timeout(v->wsi_udp, 1, LWS_TO_KILL_ASYNC);
		v->wsi_udp = NULL;
		lws_sul_schedule(v->context, 0, &v->sul, NULL,
				 LWS_SET_TIMER_USEC_CANCEL);
		break;


	/* callbacks related to raw socket descriptor */

        case LWS_CALLBACK_RAW_ADOPT:
		lwsl_user("%s: LWS_CALLBACK_RAW_ADOPT\n", __func__);
        	lws_callback_on_writable(wsi);
                break;

	case LWS_CALLBACK_RAW_CLOSE:
		lwsl_user("%s: LWS_CALLBACK_RAW_CLOSE\n", __func__);
		lws_sul_schedule(v->context, 0, &v->sul, NULL,
						 LWS_SET_TIMER_USEC_CANCEL);
		v->wsi_udp = NULL;
		break;

	case LWS_CALLBACK_RAW_RX:
		lwsl_user("%s: LWS_CALLBACK_RAW_RX (%d)\n", __func__, (int)len);
		lwsl_hexdump_notice(in, len);

		/* close the wsi */
		return -1;

	case LWS_CALLBACK_RAW_WRITEABLE:
		lwsl_user("%s: WRITEABLE\n", __func__);
		memset(pkt + LWS_PRE, 0, sizeof(pkt) - LWS_PRE);
		pkt[LWS_PRE] =  (LWSNTPC_LI_NONE << 6) |
				(LWSNTPC_VN_3 << 3) |
				(LWSNTPC_MODE_CLIENT << 0);

		if (lws_write(wsi, pkt + LWS_PRE, sizeof(pkt) - LWS_PRE, 0) !=
						  sizeof(pkt) - LWS_PRE) {
			lwsl_err("%s: Failed to write ntp client req\n",
					__func__);

			goto retry;
		}

		break;

	default:
		break;
	}

	return 0;

retry:
	lws_sul_schedule(v->context, 0, &v->sul, lws_ntpc_retry,
			 5 * LWS_US_PER_SEC);

	return -1;
}

#define LWS_PLUGIN_PROTOCOL_NTP_CLIENT \
	{ "protocol-ntpclient", callback_ntpc, 0, 128, }

#if !defined (LWS_PLUGIN_STATIC)
		
static const struct lws_protocols protocols[] = {
	LWS_PLUGIN_PROTOCOL_NTP_CLIENT
};

LWS_EXTERN LWS_VISIBLE int
init_protocol_lws_ntpc(struct lws_context *context,
		       struct lws_plugin_capability *c)
{
	if (c->api_magic != LWS_PLUGIN_API_MAGIC) {
		lwsl_err("Plugin API %d, library API %d", LWS_PLUGIN_API_MAGIC,
			 c->api_magic);
		return 1;
	}

	c->protocols = protocols;
	c->count_protocols = LWS_ARRAY_SIZE(protocols);
	c->extensions = NULL;
	c->count_extensions = 0;

	return 0;
}

LWS_EXTERN LWS_VISIBLE int
destroy_protocol_lws_ntpc(struct lws_context *context)
{
	return 0;
}

#endif
