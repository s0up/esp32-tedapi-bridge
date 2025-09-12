#!/usr/bin/env python3
import sys, os
BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(BASE, 'src', 'pypowerwall_example_reference', 'tedapi'))
from tedapi_pb2 import Message

DIN = os.environ.get('TEST_DIN','1707000-11-L--TG1250700025WH')

authA = bytes([
  0x30,0x81,0x86,0x02,0x41,0x14,0xB1,0x97,0xA5,0x7F,0xAD,0xB5,0xBA,0xD1,0x72,0x1A,
  0xA8,0xBD,0x6A,0xC5,0x18,0x98,0x30,0xB6,0x12,0x42,0xA2,0xB4,0x70,0x4F,0xB2,0x14,
  0x76,0x64,0xB7,0xCE,0x1A,0x0C,0xFE,0xD2,0x56,0x01,0x0C,0x7F,0x2A,0xF6,0xE5,0xDB,
  0x67,0x5F,0x2F,0x60,0x0B,0x16,0x95,0x5F,0x71,0x63,0x13,0x24,0xD3,0x8E,0x79,0xBE,
  0x7E,0xDD,0x41,0x31,0x12,0x78,0x02,0x41,0x70,0x07,0x5F,0xB4,0x1F,0x5D,0xC4,0x3E,
  0xF2,0xEE,0x05,0xA5,0x56,0xC1,0x7F,0x2A,0x08,0xC7,0x0E,0xA6,0x5D,0x1F,0x82,0xA2,
  0xEB,0x49,0x7E,0xDA,0xCF,0x11,0xDE,0x06,0x1B,0x71,0xCF,0xC9,0xB4,0xCD,0xFC,0x1E,
  0xF5,0x73,0xBA,0x95,0x8D,0x23,0x6F,0x21,0xCD,0x7A,0xEB,0xE5,0x7A,0x96,0xF5,0xE1,
  0x0C,0xB5,0xAE,0x72,0xFB,0xCB,0x2F,0x17,0x1F])

authB = bytes([
  0x30,0x81,0x87,0x02,0x42,0x01,0x41,0x95,0x12,0xE3,0x42,0xD1,0xCA,0x1A,0xD3,0x00,
  0xF6,0x7D,0x0B,0x45,0x40,0x2F,0x9A,0x9F,0xC0,0x0D,0x06,0x25,0xAC,0x2C,0x0E,0x6A,
  0x21,0x29,0x0A,0x64,0xEF,0xE6,0x37,0x8B,0xAF,0x62,0xD7,0xF8,0x26,0x0B,0x2E,0xC1,
  0xAC,0xD9,0x21,0x1F,0xD6,0x83,0xFF,0x6B,0x49,0x6D,0xF3,0x5C,0x4A,0xD8,0xEE,0x69,
  0x54,0x59,0xDE,0x7F,0xC5,0x78,0x52,0x02,0x41,0x1D,0x43,0x03,0x48,0xFB,0x38,0x22,
  0xB0,0xE4,0xD6,0x18,0xDE,0x11,0xC4,0x35,0xB2,0xA9,0x56,0x42,0xA6,0x4A,0x8F,0x08,
  0x9D,0xBA,0x86,0xF1,0x20,0x57,0xCD,0x4A,0x8C,0x02,0x2A,0x05,0x12,0xCB,0x7B,0x3C,
  0x9B,0xC8,0x67,0xC9,0x9D,0x39,0x8B,0x52,0xB3,0x89,0xB8,0xF1,0xF1,0x0F,0x0E,0x16,
  0x45,0xED,0xD7,0xBF,0xD5,0x26,0x29,0x92,0x2E,0x12])

status_query = " query DeviceControllerQuery {\n  control {\n    systemStatus {\n        nominalFullPackEnergyWh\n        nominalEnergyRemainingWh\n    }\n    islanding {\n        customerIslandMode\n        contactorClosed\n        microGridOK\n        gridOK\n    }\n    meterAggregates {\n      location\n      realPowerW\n    }\n    alerts {\n      active\n    },\n    siteShutdown {\n      isShutDown\n      reasons\n    }\n    batteryBlocks {\n      din\n      disableReasons\n    }\n    pvInverters {\n      din\n      disableReasons\n    }\n  }\n  system {\n    time\n    sitemanagerStatus {\n      isRunning\n    }\n    updateUrgencyCheck  {\n      urgency\n      version {\n        version\n        gitHash\n      }\n      timestamp\n    }\n  }\n}\n"

controller_query = (
  "query DeviceControllerQuery($msaComp:ComponentFilter$msaSignals:[String!]){control{systemStatus{nominalFullPackEnergyWh nominalEnergyRemainingWh}islanding{customerIslandMode contactorClosed microGridOK gridOK disableReasons}meterAggregates{location realPowerW}alerts{active}siteShutdown{isShutDown reasons}batteryBlocks{din disableReasons}pvInverters{din disableReasons}}system{time supportMode{remoteService{isEnabled expiryTime sessionId}}sitemanagerStatus{isRunning}updateUrgencyCheck{urgency version{version gitHash}timestamp}}neurio{isDetectingWiredMeters readings{firmwareVersion serial dataRead{voltageV realPowerW reactivePowerVAR currentA}timestamp}pairings{serial shortId status errors macAddress hostname isWired modbusPort modbusId lastUpdateTimestamp}}teslaRemoteMeter{meters{din reading{timestamp firmwareVersion ctReadings{voltageV realPowerW reactivePowerVAR energyExportedWs energyImportedWs currentA}}firmwareUpdate{updating numSteps currentStep currentStepProgress progress}}detectedWired{din serialPort}}pw3Can{firmwareUpdate{isUpdating progress{updating numSteps currentStep currentStepProgress progress}}enumeration{inProgress}}esCan{bus{PVAC{packagePartNumber packageSerialNumber subPackagePartNumber subPackageSerialNumber PVAC_Status{isMIA PVAC_Pout PVAC_State PVAC_Vout PVAC_Fout}PVAC_InfoMsg{PVAC_appGitHash}PVAC_Logging{isMIA PVAC_PVCurrent_A PVAC_PVCurrent_B PVAC_PVCurrent_C PVAC_PVCurrent_D PVAC_PVMeasuredVoltage_A PVAC_PVMeasuredVoltage_B PVAC_PVMeasuredVoltage_C PVAC_PVMeasuredVoltage_D PVAC_VL1Ground PVAC_VL2Ground}alerts{isComplete isMIA active}}PINV{PINV_Status{isMIA PINV_Fout PINV_Pout PINV_Vout PINV_State PINV_GridState}PINV_AcMeasurements{isMIA PINV_VSplit1 PINV_VSplit2}PINV_PowerCapability{isComplete isMIA PINV_Pnom}alerts{isComplete isMIA active}}PVS{PVS_Status{isMIA PVS_State PVS_vLL PVS_StringA_Connected PVS_StringB_Connected PVS_StringC_Connected PVS_StringD_Connected PVS_SelfTestState}PVS_Logging{PVS_numStringsLockoutBits PVS_sbsComplete}alerts{isComplete isMIA active}}THC{packagePartNumber packageSerialNumber THC_InfoMsg{isComplete isMIA THC_appGitHash}THC_Logging{THC_LOG_PW_2_0_EnableLineState}}POD{POD_EnergyStatus{isMIA POD_nom_energy_remaining POD_nom_full_pack_energy}POD_InfoMsg{POD_appGitHash}}SYNC{packagePartNumber packageSerialNumber SYNC_InfoMsg{isMIA SYNC_appGitHash SYNC_assemblyId}METER_X_AcMeasurements{isMIA isComplete METER_X_CTA_InstRealPower METER_X_CTA_InstReactivePower METER_X_CTA_I METER_X_VL1N METER_X_CTB_InstRealPower METER_X_CTB_InstReactivePower METER_X_CTB_I METER_X_VL2N METER_X_CTC_InstRealPower METER_X_CTC_InstReactivePower METER_X_CTC_I METER_X_VL3N}METER_Y_AcMeasurements{isMIA isComplete METER_Y_CTA_InstRealPower METER_Y_CTA_InstReactivePower METER_Y_CTA_I METER_Y_VL1N METER_Y_CTB_InstRealPower METER_Y_CTB_InstReactivePower METER_Y_CTB_I METER_Y_VL2N METER_Y_CTC_InstRealPower METER_Y_CTC_InstReactivePower METER_Y_CTC_I METER_Y_VL3N}}ISLANDER{ISLAND_GridConnection{ISLAND_GridConnected isComplete}ISLAND_AcMeasurements{ISLAND_VL1N_Main ISLAND_FreqL1_Main ISLAND_VL2N_Main ISLAND_FreqL2_Main ISLAND_VL3N_Main ISLAND_FreqL3_Main ISLAND_VL1N_Load ISLAND_FreqL1_Load ISLAND_VL2N_Load ISLAND_FreqL2_Load ISLAND_VL3N_Load ISLAND_FreqL3_Load ISLAND_GridState isComplete isMIA}}}enumeration{inProgress numACPW numPVI}firmwareUpdate{isUpdating powerwalls{updating numSteps currentStep currentStepProgress progress}msa{updating numSteps currentStep currentStepProgress progress}msa1{updating numSteps currentStep currentStepProgress progress}sync{updating numSteps currentStep currentStepProgress progress}pvInverters{updating numSteps currentStep currentStepProgress progress}}phaseDetection{inProgress lastUpdateTimestamp powerwalls{din progress phase}}inverterSelfTests{isRunning isCanceled pinvSelfTestsResults{din overall{status test summary setMagnitude setTime tripMagnitude tripTime accuracyMagnitude accuracyTime currentMagnitude timestamp lastError}testResults{status test summary setMagnitude setTime tripMagnitude tripTime accuracyMagnitude accuracyTime currentMagnitude timestamp lastError}}}}components{msa:components(filter:$msaComp){partNumber serialNumber signals(names:$msaSignals){name value textValue boolValue timestamp}activeAlerts{name}}}ieee20305{longFormDeviceID polledResources{url name pollRateSeconds lastPolledTimestamp}controls{defaultControl{mRID setGradW opModEnergize opModMaxLimW opModImpLimW opModExpLimW opModGenLimW opModLoadLimW}activeControls{opModEnergize opModMaxLimW opModImpLimW opModExpLimW opModGenLimW opModLoadLimW}}registration{dateTimeRegistered pin}}"
)

b_val = '{}'
b_val_alt = '{"msaComp":{"types" :["PVS","PVAC", "TESYNC", "TEPINV", "TETHC", "STSTSM",  "TEMSA", "TEPINV" ]},\n\t"msaSignals":[\n\t"MSA_pcbaId",\n\t"MSA_usageId",\n\t"MSA_appGitHash",\n\t"PVAC_Fan_Speed_Actual_RPM",\n\t"PVAC_Fan_Speed_Target_RPM",\n\t"MSA_HeatingRateOccurred",\n\t"THC_AmbientTemp",\n\t"METER_Z_CTA_InstRealPower",\n\t"METER_Z_CTA_InstReactivePower",\n\t"METER_Z_CTA_I",\n\t"METER_Z_VL1G",\n\t"METER_Z_CTB_InstRealPower",\n\t"METER_Z_CTB_InstReactivePower",\n\t"METER_Z_CTB_I",\n\t"METER_Z_VL2G"]}'

def build(name, query, code, bval, multi):
    m = Message()
    m.message.deliveryChannel = 1
    if multi:
        m.message.sender.din = DIN
    else:
        m.message.sender.local = 1
    m.message.recipient.din = DIN
    m.message.payload.send.num = 2
    m.message.payload.send.payload.value = 1
    m.message.payload.send.payload.text = query
    m.message.payload.send.code = code
    m.message.payload.send.b.value = bval
    m.tail.value = 2 if multi else 1
    blob = m.SerializeToString()
    print(f"{name}: len={len(blob)} first64=" + ' '.join(f"{b:02X}" for b in blob[:64]))
    p = os.path.join('/tmp', name + '.bin')
    with open(p,'wb') as f: f.write(blob)

if __name__ == '__main__':
    build('py_status_single', status_query, authA, b_val, False)
    build('py_status_multi', status_query, authA, b_val, True)
    build('py_controller_single', controller_query, authB, b_val_alt, False)
    build('py_controller_multi', controller_query, authB, b_val_alt, True)


