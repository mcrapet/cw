#!/bin/sh -e

test -d m4 || mkdir m4

command -v autoreconf >/dev/null || {
  echo 'configuration failed, please install autoconf first';
  exit 1;
}

autoreconf --force --install --symlink --warnings=all || {
  echo 'configuration failed, autoreconf failed';
  exit 1;
}
rm -Rf autom4te.cache

echo 'Now type:'
echo './configure'
echo 'make'
