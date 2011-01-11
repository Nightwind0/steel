#!/bin/bash
aclocal && autoheader && glibtoolize --force && automake --add-missing && autoconf
