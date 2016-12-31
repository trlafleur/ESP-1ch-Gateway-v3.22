//
// Copyright (c) 2016 Maarten Westenberg version for ESP8266
// Verison 3.2.2
// Date: 2016-12-29
//
// 	based on work done by Thomas Telkamp for Raspberry PI 1ch gateway
//	and many others.
//
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the MIT License
// which accompanies this distribution, and is available at
// https://opensource.org/licenses/mit-license.php
//
// Author: Maarten Westenberg
//
// This file contains the code for using the single channel gateway also as a sensor
// node. 
// You will have to specify the DevAddr and the AppSKey below (and on your LoRa backend).
// Also you will have to choose what sensors to forward to your application.
//
// ============================================================================
		
#if GATEWAYNODE==1

//unsigned char DevAddr[4]  = { 0x02, 0x02, 0x04, 0x20 };	// Note: byte swapping done later
unsigned char DevAddr[4]  = { 0x26, 0x01, 0x17, 0xE9 };	
unsigned char AppSKey[16] = { 0x02, 0x02, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00, 0x54, 0x68, 0x69, 0x6E, 0x67, 0x73, 0x34, 0x55 };

// ----------------------------------------------------------------------------
// XXX Experimental Read Internal Sensors
//
// You can monitor some settings of the RFM95/sx1276 chip. For example the temperature
// which is set in REGTEMP in FSK mode (not in LORA). Or the battery value.
// Find some sensible sensor
// values for LoRa radio and read them below in separate function
//
// ----------------------------------------------------------------------------
uint8_t readInternal(uint8_t reg) {
	return 0;
}

// ----------------------------------------------------------------------------
// LoRaSensors() is a function that puts sensor values in the MACPayload and 
// sends these values up to the server. For the server it is impossible to know 
// whther or not the message comes from a LoRa node or from the gateway.
//
// The example code below adds a battery value in lCode (encoding protocol) but
// of-course you can add any byte string you wish
// ----------------------------------------------------------------------------
int LoRaSensors(uint8_t *buf) {

	int internalSersors = readInternal(0x1A);
	
	buf[0] = 0x86;									// User code <lCode + len==3 + Parity
	buf[1] = 0x80;									// lCode code <battery>
	buf[2] = 0x3F;									// lCode code <value>
	return 3;										// return the number of bytes added to the payload
}

// ----------------------------------------------------------------------------
// In Sensor mode, we have to encode the user payload before sending.
// The library files for AES are added to the library directory in AES.
// For the moment we use the AES library made by ideetron as this library
// is also used in the LMIC stack.
//
// The function below follows the LoRa spec exactly
//
// NOTE:: Be aware of the LICENSE of the AES library files which is GPL3
// ----------------------------------------------------------------------------
void encodePacket(uint8_t *Data, uint8_t DataLength, uint16_t FrameCount, uint8_t Direction) {

	uint8_t i, j;
	uint8_t Block_A[16];
	uint8_t bLen=16;						// Block length is 16 except for last block in message
		
	uint8_t restLength = DataLength % 16;	// We work in blocks of 16 bytes, this is the rest
	uint8_t numBlocks  = DataLength / 16;	// Number of whole blocks to encrypt
	if (restLength>0) numBlocks++;			// And add block for the rest if any

	for(i = 1; i <= numBlocks; i++) {
		Block_A[0] = 0x01;
		
		Block_A[1] = 0x00; Block_A[2] = 0x00; Block_A[3] = 0x00; Block_A[4] = 0x00;

		Block_A[5] = Direction;				// 0 is uplink

		Block_A[6] = DevAddr[3];			// Only works for and with ABP
		Block_A[7] = DevAddr[2];
		Block_A[8] = DevAddr[1];
		Block_A[9] = DevAddr[0];

		Block_A[10] = (FrameCount & 0x00FF);
		Block_A[11] = ((FrameCount >> 8) & 0x00FF);
		Block_A[12] = 0x00; 				// Frame counter upper Bytes
		Block_A[13] = 0x00;					// These are not used so are 0

		Block_A[14] = 0x00;

		Block_A[15] = i;

		// Encrypt and calculate the S
		AES_Encrypt(Block_A, AppSKey);
		
		// Last block? set bLen to rest
		if ((i == numBlocks) && (restLength>0)) bLen = restLength;
		
		for(j = 0; j < bLen; j++) {
			*Data = *Data ^ Block_A[j];
			Data++;
		}
	}
}

// ----------------------------------------------------------------------------
// The gateway may also have local sensors that need reporting.
// We will generate a message in gateway-UDP format for upStream messaging
// so that for the backend server it seems like a LoRa node has reported a
// sensor value.
// NOTE: We do not need ANY LoRa functions here since we are on the gateway.
// We only need to send a gateway message upstream that looks like a node message.
//
// XXX NOTE:: This function does NOT encrypt the sensor (yet), however the backend
//		picks it up fine as decoder thinks it is a MAC message.
// ----------------------------------------------------------------------------
int sensorPacket(uint8_t * buff_up) {

	uint8_t mlength = 0;
	uint32_t tmst = micros();
	
	// In the next few bytes the fake LoRa message must be put
	// PHYPayload = MHDR | MACPAYLOAD | MIC
	// MHDR, 1 byte (part of MACPayload) 
	message[0] = 0x40;									// 0x40 == unconfirmed up message	
	
	// MACPayload:  FHDR + FPort + FRMPayload
	
	// FHDR consists of 4 bytes addr, 1byte Fctrl, 2bye FCnt, 0-15 byte FOpts
	//	We support ABP addresses only for Gateways
	message[1] = DevAddr[3];							// Last byte[3] of address
	message[2] = DevAddr[2];
	message[3] = DevAddr[1];
	message[4] = DevAddr[0];							// First byte[0] of Dev_Addr
	
	message[5] = 0x00;									// FCtrl is normally 0
	message[6] = frameCount % 0x100;					// LSB
	message[7] = frameCount / 0x100;					// MSB
	
	// FPort, either 0 or 1 bytes. Must be != 0 for non MAC messages such as user payload
	message[8] = 0x01;									// Port must not be 0
	mlength = 9;
	
	// FRMPayload; Payload will be AES128 encoded using AppSKey
	// See LoRa spec para 4.3.2
	// You can add any byte string below based on you personal
	// choice of sensors etc.
	//
	// Payload bytes in this example are encoded in the LoRaCode(c) format
	uint8_t PayLength = LoRaSensors((uint8_t *)(message+mlength));
	
	// we have to include the AES functions at this stage in order to generate LoRa Payload.
	encodePacket((uint8_t *)(message+mlength), PayLength, (uint16_t)frameCount, 0);
	
	mlength += PayLength;								// length inclusive sensor data	
	mlength += 4;										// LMIC Not Used but we have to add MIC bytes to PHYPayload
	
	frameCount++;
	
	Serial.println(F("sensorPacket"));
	int buff_index = buildPacket(tmst, buff_up, message, mlength, true);
	return(buff_index);
}


#endif //GATEWAYNODE==1