#!/bin/bash
#
TEST="Test unsupported PCC-backed power-meter methods"
NAME=test-0001.sh
TMPLOG=$TMP/method-pcc.log.$$
TMPRAW=$TMP/method-pcc.raw.$$

if ! $FWTS --show-tests | grep -q method; then
	echo SKIP: $TEST, $NAME
	exit 77
fi

trap 'rm -f "$TMPLOG" "$TMPRAW"' EXIT

$FWTS --log-format="%owner " -w 200 \
	--dumpfile="$FWTSTESTDIR/method-pcc-0001/acpidump-0001.log" method - > "$TMPRAW"
# The fixture deliberately contains genuine firmware errors.
if [[ $? -ne 1 ]]; then
	cat "$TMPRAW" >> "$FAILURE_LOG"
	echo FAILED: $TEST, $NAME
	exit 1
fi

# Check PCC skips and ordinary methods, and retain every firmware failure so
# that unexpected failures (including lock accounting errors) fail this test.
grep '^method ' "$TMPRAW" | \
	grep -E 'PCC OperationRegion|FAILED|GOOD.*correctly returned an integer' > "$TMPLOG"
diff "$TMPLOG" "$FWTSTESTDIR/method-pcc-0001/method-0001.log" >> "$FAILURE_LOG"
ret=$?
if [[ $ret -eq 0 ]]; then
	echo PASSED: $TEST, $NAME
else
	echo FAILED: $TEST, $NAME
fi
exit $ret
