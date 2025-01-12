#!/bin/sh

gcc -shared -fPIC -o interceptor.so interceptor.c -ldl
# doesnt get simpler than this
