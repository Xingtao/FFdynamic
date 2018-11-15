#!/bin/bash -x

curl -v http://127.0.0.1:14444/unittest

# in case iresty_test is not installed, using the following ones
# curl -v GET http://127.0.0.1:8444/rest/v1/new_task
# curl -v http://127.0.0.1:8444/rest/v1/delete_task -d @update_testcase_1.json
