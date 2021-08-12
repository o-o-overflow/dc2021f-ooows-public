#!/usr/bin/env bash

VMM_FD=3
RAM_FD=4
SYS_FD=5

LANG=C
LC_ALL=C
DEBUG=false

trap "echo EXITING" EXIT
exec 11</dev/zero

function read_bytes {
	read -u $1 -r -d '' -N $3 $2
	eval '_LEN=${#'$2'}'
	$DEBUG && echo -n "Received $_LEN bytes into $2: " && eval 'binprint "$'$2'"'
}

function read_num {
	if [ $3 -ne 0 ]
	then
		_NUM=0x$(head -c $3 <&3 | od -tx1 -An -v -w$3 | tr ' ' '\n' | tac | tr -d '\n')
	else
		_NUM=0x00
	fi
	eval $2=$(($_NUM+0))
	$DEBUG && echo Received $2=$_NUM
}

function sendfile {
	dd bs=$4 skip=$3 count=1 <&$2 >&$1 2>/dev/null
}

function write_bytes {
	[ -n "$3" ] && echo -E -n "${2:0:$3}" || echo -E -n "$2" >&$1
}

function binprint {
	echo -En "$1" | od -tx1 -An -v -w999
}

$DEBUG && echo DOING HANDSHAKE
write_bytes $VMM_FD "INIT"
read_bytes $VMM_FD ACK 4
if [ "$ACK" == "TINI" ]
then
	$DEBUG && echo HANDSHAKE COMPLETE
else
	echo "HANDSHAKE FAILED (got $ACK instead of TINI)"
fi

read_num $VMM_FD CPU0_FD 4
read_num $VMM_FD CPU1_FD 4
read_num $VMM_FD CPU2_FD 4
read_num $VMM_FD CPU3_FD 4

FILENAME=""
FILTER_STRING="flag"
FILTER_OFFSET=0

while true
do
	read_num $VMM_FD RQ_TYPE 4
	if [ "$RQ_TYPE" -eq 0 ]
	then
		$DEBUG && echo "Handling PIO request."
		read_num $VMM_FD PIO_PORT 2
		read_num $VMM_FD PIO_DIRECTION 1
		read_num $VMM_FD PIO_SIZE 1
		read_num $VMM_FD PIO_DATA $PIO_SIZE
		read_num $VMM_FD PIO_EXTRA $((4-$PIO_SIZE))
		read_num $VMM_FD PIO_COUNT 4
		read_num $VMM_FD PIO_EXTRA $((28-4-2-1-1-4-4))

		if [ "$PIO_DIRECTION" -eq 1 ]
		then
			if [ "$PIO_DATA" -eq 1337 ]
			then
				$DEBUG && echo "Opening the file!"
				exec 137<$FILENAME
				FILENAME=""
				write_bytes $VMM_FD "ohya"
			else
				eval "BYTE=$'\x"$(printf %02x $PIO_DATA)"'"
				FILENAME="$FILENAME$BYTE"
				$DEBUG && echo "Added BYTE=$BYTE to FILENAME=$FILENAME"

				if [ "${FILTER_STRING:$FILTER_OFFSET:1}" == "$BYTE" ]
				then
					$DEBUG && echo "Byte $BYTE (offset $FILTER_OFFSET) matched!"
					FILTER_OFFSET=$(($FILTER_OFFSET+1))
					if [ $FILTER_OFFSET -ge ${#FILTER_STRING} ]
					then
						FILTER_OFFSET=0
						FILENAME=""
					fi
				else
					FILTER_OFFSET=0
				fi
				write_bytes $VMM_FD "ohya"
			fi
		else
			$DEBUG && echo "Got input request for size $PIO_SIZE"
			sendfile $VMM_FD 137 0 $PIO_SIZE
			[ $PIO_SIZE -lt 4 ] && sendfile $VMM_FD 11 0 $((4-$PIO_SIZE))
		fi
	else
		$DEBUG && echo "Unsupported RQ_TYPE=$RQ_TYPE..."
	fi
done
