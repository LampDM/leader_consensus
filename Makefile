PROGRAM=nrftest
PROGRAM_SRC_FILES=./receiver.cpp
EXTRA_COMPONENTS=extras/i2c extras/rboot-ota extras/RF24 extras/paho_mqtt_c
include ../../common.mk
