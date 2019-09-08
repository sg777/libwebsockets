# lws minimal ws server raw ntpclient

This example demonstrates talking to an ntp server.

## build

```
 $ cmake . && make
```

## usage

```
[2019/09/09 09:57:19:7811] U: LWS minimal ntpclient
[2019/09/09 09:57:19:8583] N: LWS_CALLBACK_PROTOCOL_INIT
[2019/09/09 09:57:19:8635] N: lws_ntpc_retry: server pool.ntp.org
[2019/09/09 09:57:19:9230] U: callback_ntpc: LWS_CALLBACK_RAW_ADOPT
[2019/09/09 09:57:19:9252] U: callback_ntpc: WRITEABLE
[2019/09/09 09:57:19:9429] U: callback_ntpc: LWS_CALLBACK_RAW_RX (48)
[2019/09/09 09:57:19:9437] N: 
[2019/09/09 09:57:19:9469] N: 0000: 1C 02 03 E9 00 00 00 CF 00 00 00 C7 55 C7 D6 62    ............U..b
[2019/09/09 09:57:19:9472] N: 0010: E1 20 92 62 4B F3 FA 0E 00 00 00 00 00 00 00 00    . .bK...........
[2019/09/09 09:57:19:9474] N: 0020: E1 20 92 6F 2F E4 D1 94 E1 20 92 6F 2F EA 0E EE    . .o/.... .o/...
[2019/09/09 09:57:19:9477] N: 
[2019/09/09 09:57:19:9555] U: callback_ntpc: LWS_CALLBACK_RAW_CLOSE
^C
```

