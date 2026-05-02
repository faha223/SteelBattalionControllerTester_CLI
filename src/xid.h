#ifndef __XID_H__
#define __XID_H__

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

#define CAPCOM_VID 0x0a7b
#define MICROSOFT_VID 0x045E
#define STEEL_BATTALION_CONTROLLER_PID 0xd000
#define XBOX_CONTROLLER_PID 0x0289

#define USE_ASYNC_API 1
#define INTERRUPT_TIMEOUT 0
#define ENDPOINT_OUT 0x01
#define ENDPOINT_IN 0x82

static const char *status_str(int status);

typedef struct __attribute__((packed))
{
	// W0
	uint16_t MainWeapon : 1;
	uint16_t Fire : 1;
	uint16_t LockOn : 1;
	uint16_t Eject : 1;
	uint16_t CockpitHatch : 1;
	uint16_t Ignition : 1;
	uint16_t Start : 1;
	uint16_t MultiMonitorOpenClose : 1;
	uint16_t MultiMonitorMapZoomInOut : 1;
	uint16_t MultiMonitorModeSelect : 1;
	uint16_t MultiMonitorSubMonitor : 1;
	uint16_t MainMonitorZoomIn : 1;
	uint16_t MainMonitorZoomOut : 1;
	uint16_t ForecastShootingSystem : 1;
	uint16_t Manipulator : 1;
	uint16_t LineColorChange : 1;

	// W1
	uint16_t Washing : 1;
	uint16_t Extinguisher : 1;
	uint16_t Chaff : 1;
	uint16_t TankDetach : 1;
	uint16_t Override : 1;
	uint16_t NightScope : 1;
	uint16_t Function1 : 1;
	uint16_t Function2 : 1;
	uint16_t Function3 : 1;
	uint16_t WeaponConMain : 1;
	uint16_t WeaponConSub : 1;
	uint16_t WeaponConMagazine : 1;
	uint16_t Comm1 : 1;
	uint16_t Comm2 : 1;
	uint16_t Comm3 : 1;
	uint16_t Comm4 : 1;

	// W2
	uint16_t Comm5 : 1;
	uint16_t SightChange : 1;
	uint16_t ToggleFiltControl : 1;
	uint16_t ToggleOxygenSupply : 1;
	uint16_t ToggleFuelFlowRate : 1;
	uint16_t ToggleBufferMaterial : 1;
	uint16_t ToggleVTLocation : 1;
	uint16_t notUsed : 9;
} USB_SteelBattalion_Buttons;

typedef struct __attribute__((packed))
{
	uint8_t zero;
	uint8_t bLength;
	USB_SteelBattalion_Buttons dButtons;
	uint16_t aimingX;       //0 to 2^16 left to right
	uint16_t aimingY;       //0 to 2^16 top to bottom
	int16_t rotationLever;
	int16_t sightChangeX;
	int16_t sightChangeY;
	uint16_t leftPedal;      //Sidestep, 0x0000 to 0xFF00
	uint16_t middlePedal;    //Brake, 0x0000 to 0xFF00
	uint16_t rightPedal;     //Acceleration, 0x0000 to oxFF00
	int8_t tunerDial;        //0-15 is from 9oclock, around clockwise
	int8_t gearLever;        //7-13 is gears R,1,2,3,4,5
} USB_SteelBattalion_InReport_t;

typedef struct
{
	uint8_t bReportId;
	uint8_t bLength;
	uint8_t EmergencyExit : 4;
	uint8_t CockpitHatch : 4;
	uint8_t Ignition : 4;
	uint8_t Start : 4;
	uint8_t OpenClose : 4;
	uint8_t MapZoomInOut : 4;
	uint8_t ModeSelect : 4;
	uint8_t SubMonitorModeSelect : 4;
	uint8_t MainMonitorZoomIn : 4;
	uint8_t MainMonitorZoomOut : 4;
	uint8_t ForecastShootingSystem : 4;
	uint8_t Manipulator : 4;
	uint8_t LineColorChange : 4;
	uint8_t Washing : 4;
	uint8_t Extinguisher : 4;
	uint8_t Chaff : 4;
	uint8_t TankDetach : 4;
	uint8_t Override : 4;
	uint8_t NightScope : 4;
	uint8_t FunctionF1 : 4;
	uint8_t FunctionF2 : 4;
	uint8_t FunctionF3 : 4;
	uint8_t MainWeaponControl : 4;
	uint8_t SubWeaponControl : 4;
	uint8_t MagazineChange : 4;
	uint8_t Comm1 : 4;
	uint8_t Comm2 : 4;
	uint8_t Comm3 : 4;
	uint8_t Comm4 : 4;
	uint8_t Comm5 : 4;
	uint8_t _notUsed : 4;
	uint8_t GearR : 4;
	uint8_t GearN : 4;
	uint8_t Gear1 : 4;
	uint8_t Gear2 : 4;
	uint8_t Gear3 : 4;
	uint8_t Gear4 : 4;
	uint8_t Gear5 : 4;
	uint8_t _notUsed2;
} USB_SteelBattalion_OutReport_t;

typedef struct
{
	uint8_t bReportId;
	uint8_t bLength;

	uint8_t bDpadUp : 1;
	uint8_t bDpadDown : 1;
	uint8_t bDpadLeft : 1;
	uint8_t bDpadRight : 1;
	uint8_t bStart : 1;
	uint8_t bBack : 1;
	uint8_t bLeftStick : 1;
	uint8_t bRightStick : 1;

	uint8_t bReserved;

	uint8_t bA;
	uint8_t bB;
	uint8_t bX;
	uint8_t bY;
	uint8_t bBlack;
	uint8_t bWhite;
	uint8_t bLeftTrigger;
	uint8_t bRightTrigger;
	int16_t sLeftStickX;
	int16_t sLeftStickY;
	int16_t sRightStickX;
	int16_t sRightStickY;
} USB_XboxGamepad_InReport_t;

typedef struct
{
	uint8_t bReportId;
	uint8_t bLength;
	uint16_t bLeftActuator;
	uint16_t bRightActuator;
} USB_XboxGamepad_OutReport_t;

#endif
