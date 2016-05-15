#!/usr/bin/python
# -*- coding: UTF-8 -*-

# 输出系统时间

import time

print "Content-type:text/html\r\n\r\n"
localtime = time.asctime( time.localtime(time.time()) )
print "本地时间为 :"+localtime
