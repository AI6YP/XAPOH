all : flash

TARGET:=main

TARGET_MCU?=CH32V003
include ../../../../cnlohr/ch32fun/ch32fun/ch32fun.mk

noreset :
	$(MINICHLINK)/minichlink -D

flash : cv_flash
clean : cv_clean


