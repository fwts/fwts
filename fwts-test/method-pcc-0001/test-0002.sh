#!/bin/bash
#
TEST="Test PCC state consumption and unrelated AE_SUPPORT reporting"
NAME=test-0002.sh
TMPLOG=$TMP/method-pcc-state.log.$$

trap 'rm -f "$TMPLOG"' EXIT

"$PCC_STATE_TEST" "$FWTSTESTDIR/method-pcc-0001/acpidump-0001.log" > "$TMPLOG" 2>&1
ret=$?
if [[ $ret -eq 0 ]]; then
	echo PASSED: $TEST, $NAME
else
	cat "$TMPLOG" >> "$FAILURE_LOG"
	echo FAILED: $TEST, $NAME
fi
exit $ret
