#guilbert.lee@lge.com Mon 28 Jan 2013
#lg dtb parser

#!/bin/sh

WORK_PATH=${0%/*}
dtb_name=""

DTB_LIST=$(echo $(find . -name '*.dtb'))
echo $DTB_LIST

if [ "$DTB_LIST" = "" ]
then
    echo "Cannot find *.dtb file in current folder."

    exit
fi

dtb_name=$(echo $DTB_LIST | tr " " ";")
dtb_name=$(echo $dtb_name | tr ";" "\n")

for x in $dtb_name
do
    DTB_NAME=${x##*/}
    DTS_NAME=${DTB_NAME/%.dtb/}
    DTS_NAME=$DTS_NAME.dts
    ${WORK_PATH}/lg_dtc -I dtb -O dts ${WORK_PATH}/$DTB_NAME -s > ./$DTS_NAME
done