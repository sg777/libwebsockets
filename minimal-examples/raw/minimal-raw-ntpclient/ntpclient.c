/*
 * lws-minimal-raw-ntpclient
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates integrating a connected udp
 * socket into the lws event loop as a RAW wsi.  It's interesting in
 * the kind of situation where you already have a connected socket
 * in your application, and you need to hand it over to lws to deal with.
 *
 * Lws supports "adopting" these foreign sockets, and also has a helper API
 * to create, bind, and adopt them inside lws.
 */

#include <libwebsockets.h>
#include <string.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define LWS_PLUGIN_STATIC
#include "protocol_ntpclient.c"

static const struct lws_protocols protocols[] = {
	LWS_PLUGIN_PROTOCOL_NTP_CLIENT,
	{ }
};

static struct lws_protocol_vhost_options pvo1 = {
        NULL, NULL, "server", "pool.ntp.org"
}, pvo = {
        NULL, &pvo1, "protocol-ntpclient", ""
};


static int interrupted;

void sigint_handler(int sig)
{
	interrupted = 1;
}

int main(int argc, const char **argv)
{
	struct lws_context_creation_info info;
	struct lws_context *context;
	const char *p;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

	signal(SIGINT, sigint_handler);

	if ((p = lws_cmdline_option(argc, argv, "-d")))
		logs = atoi(p);

	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal ntpclient\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.pvo = &pvo;
	info.port = CONTEXT_PORT_NO_LISTEN_SERVER;
	info.protocols = protocols;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);

	return 0;
}
