#!/bin/sh -e

test -d m4 || mkdir m4
autoreconf --force --install --symlink --warnings=all || {
  echo 'autogen.sh failed';
  exit 1;
}
rm -Rf autom4te.cache

echo 'Now type:'
echo './configure'
echo 'make'
