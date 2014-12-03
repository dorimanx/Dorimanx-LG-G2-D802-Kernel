#!/bin/sh

ZIPS_IN_FOLDER=$(ls -la *.zip | wc -l);
if [ $ZIPS_IN_FOLDER -eq "0" ]; then
	echo "Please add ONE not bumped recovery ZIP to this folder!"
	exit 1;
elif [ "$ZIPS_IN_FOLDER" -ge "2" ]; then
	echo "Only one ZIP allowed to be in this work folder! leave only ONE that you wish to BUMP!"
	exit 1;
fi;

PYTHON_CHECK=$(ls -la /usr/bin/python2 | wc -l);
if [ "$PYTHON_CHECK" -eq "1" ]; then
	rm -rf temp_recovery/* > /dev/null
	unzip *.zip -d temp_recovery/
	/usr/bin/python2 open_bump.py temp_recovery/recovery.img;
	mv temp_recovery/recovery_bumped.img bumped_recovery/recovery.img;
	echo "Recovery Bumped"
	cd bumped_recovery/
	zip -r bumped_recovery_twrp.zip *
	mv bumped_recovery_twrp.zip ../READY_RECOVERY
	cd ..
	rm -rf temp_recovery/* > /dev/null
	touch temp_recovery/EMPTY_DIRECTORY
else
	echo "you dont have PYTHON2.x script will not work!!!";
	exit 1;
fi;
