# Dual-monitor client use

This branch builds Windows x64. Enable dual-monitor mode for an extended desktop;
a single local monitor automatically uses the normal single-monitor request.

A Galaxy Book with a Galaxy Tab connected as a Windows Second Screen is one
Windows client with two monitors. Run Moonlight on the Galaxy Book. The tablet
does not need a second Moonlight session in that configuration. Use Windows
Extend, rather than Duplicate, before connecting.

Independent dual and single clients require the corresponding Sunshine
multi-client capture implementation. The client already sends its own monitor
layout. While another client is connected, host physical resolutions remain fixed
and Sunshine fits each client's video separately. A shared desktop still shares
the actual host cursor, keyboard and application windows.

Android is a separate codebase. Do not install this Windows package on Android.
