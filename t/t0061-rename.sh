#!/bin/sh
#
# Copyright (c) 2009 Johannes E. Schindelin
#

test_description='Test renaming'

. ./test-lib.sh

test_expect_success setup '
	test_commit initial Hello
'

test_expect_success 'case-changing rename' '
	git mv Hello hello &&
	test_commit hello &&
	test "hello" = "$(ls)"
'

test_done
