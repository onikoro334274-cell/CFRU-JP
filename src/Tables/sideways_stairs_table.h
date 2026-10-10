/* 斜めの階段: 素の動きの表（日本語版 0x0836A644・0xAA 個）の中身。scripts/generate_sideways_stairs.py が生成・手で編集しないこと */
#pragma once

#define SIDEWAYS_VANILLA_MOVEMENT_ACTIONS 0xAA

#define SIDEWAYS_VANILLA_MOVEMENT_ACTION_TABLE \
	(const void*) 0x0836A8EC, (const void*) 0x0836A8F4, (const void*) 0x0836A8FC, (const void*) 0x0836A904, \
	(const void*) 0x0836AAB8, (const void*) 0x0836AAC0, (const void*) 0x0836AAC8, (const void*) 0x0836AAD0, \
	(const void*) 0x0836A950, (const void*) 0x0836A95C, (const void*) 0x0836A968, (const void*) 0x0836A974, \
	(const void*) 0x0836A98C, (const void*) 0x0836A980, (const void*) 0x0836A998, (const void*) 0x0836A9A4, \
	(const void*) 0x0836A9B0, (const void*) 0x0836A9BC, (const void*) 0x0836A9C8, (const void*) 0x0836A9D4, \
	(const void*) 0x0836A9EC, (const void*) 0x0836A9F8, (const void*) 0x0836AA04, (const void*) 0x0836AA10, \
	(const void*) 0x0836AA1C, (const void*) 0x0836AA28, (const void*) 0x0836AA34, (const void*) 0x0836AA40, \
	(const void*) 0x0836AA4C, (const void*) 0x0836AA58, (const void*) 0x0836AA64, (const void*) 0x0836AA70, \
	(const void*) 0x0836AA7C, (const void*) 0x0836AAD8, (const void*) 0x0836AAE4, (const void*) 0x0836AAF0, \
	(const void*) 0x0836AAFC, (const void*) 0x0836AB08, (const void*) 0x0836AB14, (const void*) 0x0836AB20, \
	(const void*) 0x0836AB2C, (const void*) 0x0836AB38, (const void*) 0x0836AB44, (const void*) 0x0836AB50, \
	(const void*) 0x0836AB5C, (const void*) 0x0836AB68, (const void*) 0x0836AB74, (const void*) 0x0836AB80, \
	(const void*) 0x0836AB8C, (const void*) 0x0836AB98, (const void*) 0x0836ABA4, (const void*) 0x0836ABB0, \
	(const void*) 0x0836ABBC, (const void*) 0x0836ABC8, (const void*) 0x0836ABD4, (const void*) 0x0836ABE0, \
	(const void*) 0x0836ABEC, (const void*) 0x0836ABF8, (const void*) 0x0836AC04, (const void*) 0x0836AC10, \
	(const void*) 0x0836AC1C, (const void*) 0x0836AC28, (const void*) 0x0836AC34, (const void*) 0x0836AC40, \
	(const void*) 0x0836AC4C, (const void*) 0x0836AC58, (const void*) 0x0836AC64, (const void*) 0x0836AC70, \
	(const void*) 0x0836AC7C, (const void*) 0x0836AC88, (const void*) 0x0836AC94, (const void*) 0x0836ACA0, \
	(const void*) 0x0836ACAC, (const void*) 0x0836ACB8, (const void*) 0x0836ACF4, (const void*) 0x0836ACFC, \
	(const void*) 0x0836AD04, (const void*) 0x0836AD0C, (const void*) 0x0836AD14, (const void*) 0x0836AD20, \
	(const void*) 0x0836AD2C, (const void*) 0x0836AD38, (const void*) 0x0836AD44, (const void*) 0x0836AD50, \
	(const void*) 0x0836AD5C, (const void*) 0x0836AD68, (const void*) 0x0836AD74, (const void*) 0x0836AD80, \
	(const void*) 0x0836AD8C, (const void*) 0x0836AD98, (const void*) 0x0836ADA4, (const void*) 0x0836ADAC, \
	(const void*) 0x0836ADB8, (const void*) 0x0836ADC0, (const void*) 0x0836ADC8, (const void*) 0x0836ADD0, \
	(const void*) 0x0836ADD8, (const void*) 0x0836ADE0, (const void*) 0x0836ADE8, (const void*) 0x0836ADF0, \
	(const void*) 0x0836ADF8, (const void*) 0x0836AE00, (const void*) 0x0836AE08, (const void*) 0x0836AE10, \
	(const void*) 0x0836AE1C, (const void*) 0x0836AE2C, (const void*) 0x0836AE3C, (const void*) 0x0836AE44, \
	(const void*) 0x0836AE4C, (const void*) 0x0836AE54, (const void*) 0x0836AE5C, (const void*) 0x0836AE68, \
	(const void*) 0x0836AE74, (const void*) 0x0836AE7C, (const void*) 0x0836AE84, (const void*) 0x0836AE8C, \
	(const void*) 0x0836AE94, (const void*) 0x0836AEA0, (const void*) 0x0836AEAC, (const void*) 0x0836AEB8, \
	(const void*) 0x0836AEC4, (const void*) 0x0836AED0, (const void*) 0x0836AEDC, (const void*) 0x0836AEE8, \
	(const void*) 0x0836AF24, (const void*) 0x0836AF30, (const void*) 0x0836AF3C, (const void*) 0x0836AF48, \
	(const void*) 0x0836AF54, (const void*) 0x0836AF60, (const void*) 0x0836AF6C, (const void*) 0x0836AF78, \
	(const void*) 0x0836AF84, (const void*) 0x0836AF90, (const void*) 0x0836AF9C, (const void*) 0x0836AFA8, \
	(const void*) 0x0836AFB4, (const void*) 0x0836AFC0, (const void*) 0x0836AFCC, (const void*) 0x0836AFD8, \
	(const void*) 0x0836AFE4, (const void*) 0x0836AFF0, (const void*) 0x0836AFFC, (const void*) 0x0836B008, \
	(const void*) 0x0836B014, (const void*) 0x0836B020, (const void*) 0x0836B02C, (const void*) 0x0836B038, \
	(const void*) 0x0836B044, (const void*) 0x0836B050, (const void*) 0x0836B05C, (const void*) 0x0836B068, \
	(const void*) 0x0836B074, (const void*) 0x0836B07C, (const void*) 0x0836B084, (const void*) 0x0836A920, \
	(const void*) 0x0836A92C, (const void*) 0x0836A938, (const void*) 0x0836A944, (const void*) 0x0836B08C, \
	(const void*) 0x0836AA88, (const void*) 0x0836AA94, (const void*) 0x0836AAA0, (const void*) 0x0836AAAC, \
	(const void*) 0x0836B094, (const void*) 0x0836B0A0, (const void*) 0x0836ACC4, (const void*) 0x0836ACD0, \
	(const void*) 0x0836ACDC, (const void*) 0x0836ACE8

