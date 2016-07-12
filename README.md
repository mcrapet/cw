cURL wrapper
============

*cURL wrapper* (alias cw) is an attempt for parsing [cURL](https://curl.haxx.se/)'s output transfer statistics.
I wanted it as tiny as possible, that's why it's written using C language.

```
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
 15 20.0M   15 3125k    0     0  2279k      0  0:00:08  0:00:01  0:00:07 2279k
```

It is a set of (standalone) commandline tools containing:
- *c2z*: Frontend using [Zenity](https://wiki.gnome.org/Projects/Zenity) (progress bar widget)
- *cw*: Unix pipe filter command

This software is still very beta. I'll gradually improve it over time.

CLI usage
---------

Replace cURL command. Arguments are given verbatim to `curl` program and `zenity` is launched.

```sh
$ c2z http://www.foo1234.com/20MiB.tar -o 20MiB.tar
```

**Note**: curl's simple progress bar switch (`-#`) is handled too.

Parse statistics coming from stdin and write results on stdout:

```sh
$ curl http://www.foo1234.com/20MiB.tar -O -J 2>&1 | cw
1
# 1% (784k/s)
14
# 14% (2199k/s)
31
# 31% (2661k/s)
47
# 47% (2872k/s)
64
# 64% (3002k/s)
88
# 88% (3566k/s)
100
```

License
-------

cw is available under the terms of the GNU General Public License, Version 3.
Please check the LICENSE file for further details.
