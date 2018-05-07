// TTL_N.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "NIDAQmx.h"
#include <string>
#include <Windows.h>
#include "TTL_N.h"


static HANDLE SOT = NULL;
static HANDLE EOT = NULL;
static HANDLE SOTThread = NULL;
static BOOL WaitingForSOT = FALSE;
static BOOL WaitingForEOT = FALSE;
static UINT ActivateSite = 0x0000000F;

//short listen = 0;
bool getHandler = TRUE;
//------ for debug--------------------------------
int hardBins[4] = { 5,6,6,6 };
char *Fullsites = "Fullsites 0000000E";

TaskHandle	taskHandleInitial = 0;
TaskHandle	taskHandleSOT = 0;
TaskHandle	taskHandleEOT0 = 0;//site1   
TaskHandle	taskHandleEOT1 = 0;//site2
TaskHandle	taskHandleEOT2 = 0;//site3
TaskHandle	taskHandleEOT3 = 0;//site4

uInt8 DataSot[8] = { 0 };
int numSampsPerChan = 10;
int32 sampsPerChanRead[1] = { 10 };
//--------Information---------------------------------------------------
//device name "Dev1"

//unsigned long  T21 = 3; //ms
//unsigned long  T43 = 3; //ms
//unsigned long  T54 = 6; //ms
//unsigned long  T63 = 12; //ms
//unsigned long  T65 = 3; //ms

unsigned long  T21 = 10; //ms
unsigned long  T43 = 5; //ms
unsigned long  T54 = 10; //ms
unsigned long  T63 = 20; //ms
unsigned long  T65 = 5; //ms

//unsigned long  T21 = 1; //ms
//unsigned long  T43 = 1; //ms
//unsigned long  T54 = 2; //ms
//unsigned long  T63 = 4; //ms
//unsigned long  T65 = 1; //ms

int32 NumberOfSite = 4;
float64 SotWaitingtime = T21 *1e-3; //s
uInt8 SOTdata[32] = { -1,-1,-1,-1 };
int32 numRead;
int32 bytesPerSamp;
int GetActiveSite[4] = { 1,1,1,1 };
int AllActiveSite = 0;
uInt8 Initdata[32] = { 0 };

uInt16      DataEot[4] = { 0x0000,0x0000,0x0000,0x0000 };
uInt16      DataBin[4] = { 0x0000,0x0000,0x0000,0x0000 };
uInt16      DataReset[4] = { 0xFFFF,0xFFFF,0xFFFF,0xFFFF };
uInt8       DataSOT[1] = { 0xFF};
//----------------------------------------------------------------------

int Delay_ms(double time)
{
	bool timeout = true;
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	double gettime = 0;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	while (timeout)
	{
		//for (int i = 0; i < 100; i++)
		//{
		//	int m = 0;
		//}
		QueryPerformanceCounter(&end);
		gettime = ((double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart)*1e3;
		if (gettime >= time)
		{
			timeout = false;
		}
	}
	return 0;
}


static BOOL ProcessActiveSites(const char* fullSites)
{
	ActivateSite = 0x0000000F;    // Default to maximum available sites.
	BOOL validResponse = FALSE;
	char fullSiteStr[20];
	sscanf_s(fullSites, "%s%x", fullSiteStr, sizeof(fullSiteStr), &ActivateSite);
	if (_stricmp("Fullsites", fullSiteStr) == 0)
	{
		validResponse = TRUE;
	}
	else
	{
		//message: Fullsites don't macth
	}
	return validResponse;
}

DWORD WINAPI SOTMonitor(LPVOID nothing)//static DWORD WINAPI SotMonitor(LPVOID nothing)
{
	BOOL WaitingForResponse;
	int32 written;
	int status = -1;
	int SOTstatus = -1;
	while (WaitingForSOT)
	{
		if (EOT)
		{
			WaitForSingleObject(EOT, INFINITE);
			//------------check handler connect site----------------------------------------
			status = DAQmxStartTask(taskHandleInitial);
			status = DAQmxReadDigitalLines(taskHandleInitial, 1, 2e-3, DAQmx_Val_GroupByChannel, &Initdata[0], 32, &numRead, &bytesPerSamp, NULL);
			status = DAQmxStopTask(taskHandleInitial);

			status = DAQmxStartTask(taskHandleSOT);
			
		}
		//status = DAQmxWriteDigitalU8(taskHandleInitial, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataSOT[0], &written, NULL);
		SOTstatus = -1;
		//SetEvent(SOT);
		GetActiveSite[0] = GetActiveSite[1] = GetActiveSite[2] = GetActiveSite[3] = 1;
		WaitingForResponse = TRUE;
		while (WaitingForSOT && WaitingForResponse)
		{
			SOTstatus = DAQmxReadDigitalLines(taskHandleSOT, NumberOfSite, SotWaitingtime, DAQmx_Val_GroupByChannel, &SOTdata[0], 32, &numRead, &bytesPerSamp, NULL);
			if (numRead >= 1)
			{
				for (int i = 0; i < NumberOfSite; i++)
				{
					for (int m = 0; m < numRead; m++)
					{
						GetActiveSite[i] &= SOTdata[numRead*i + m];
					}
				}
				for (int i = 0; i < NumberOfSite; i++)
				{
					GetActiveSite[i] = !GetActiveSite[i] & Initdata[i];
				}
				if (GetActiveSite[0] || GetActiveSite[1] || GetActiveSite[2] || GetActiveSite[3])
				{
					WaitingForResponse = false;
				}
			}
		}
		if (WaitingForSOT && !WaitingForResponse)
		{
			//printf("DataSot\n");//debug
			SetEvent(SOT);
			status = DAQmxStopTask(taskHandleSOT);
		}
		else
		{
			status = DAQmxStopTask(taskHandleSOT);
		}
	}
	return 0;
}

extern "C" HANDLER_API int EOTProcess(int* hardBins)
{
	int status = -1;
	//hardBins[4] = { 8,9,9,9 };

	int32 written;

	// 4 site

	for (int i = 0; i < 4; i++)
	{
		if (GetActiveSite[i])
		{
			DataBin[i] = pow(2, hardBins[i]);
			DataEot[i] = DataBin[i] + 1;
		}
		else
		{
			DataBin[i] = 0;
			DataEot[i] = 0;
		}
		DataBin[i] = ~DataBin[i];
		DataEot[i] = ~DataEot[i];
	
	}


	status = DAQmxWriteDigitalU16(taskHandleEOT0, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[0], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT1, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[1], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT2, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[2], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT3, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[3], &written, NULL);
	Delay_ms(T43);

	status = DAQmxWriteDigitalU16(taskHandleEOT0, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataEot[0], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT1, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataEot[1], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT2, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataEot[2], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT3, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataEot[3], &written, NULL);
	Delay_ms(T54);

	status = DAQmxWriteDigitalU16(taskHandleEOT0, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[0], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT1, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[1], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT2, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[2], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT3, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataBin[3], &written, NULL);
	Delay_ms(T65);

	status = DAQmxWriteDigitalU16(taskHandleEOT0, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[0], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT1, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[1], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT2, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[2], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT3, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[3], &written, NULL);


	//--------set EOT --------------------------------------------------------------------------------------------
	SetEvent(EOT);
	//printf("%d", status);
	status = 10101010;
	return status;
}


//EXTERN_C HANDLER_API int Setup()
extern "C" HANDLER_API int Setup()
{
	/*TaskHandle	taskHandle = 0;*/
	uInt32		data;
	int status = 0;

	status |= DAQmxCreateTask("", &taskHandleInitial);//for site1
	status |= DAQmxCreateTask("", &taskHandleSOT);
	status |= DAQmxCreateTask("", &taskHandleEOT0);//for site1
	status |= DAQmxCreateTask("", &taskHandleEOT1);//for site2
	status |= DAQmxCreateTask("", &taskHandleEOT2);//for site3
	status |= DAQmxCreateTask("", &taskHandleEOT3);//for site4


	status |= DAQmxCreateDIChan(taskHandleSOT, "Dev1/port0/line0:3", "", DAQmx_Val_ChanPerLine);
	status |= DAQmxCfgChangeDetectionTiming(taskHandleSOT, "", "Dev1/port0/line0:3", DAQmx_Val_ContSamps, NumberOfSite);
	status |= DAQmxSetDIDigFltrEnable(taskHandleSOT, "Dev1/port0/line0:3", 0);
	status |= DAQmxSetDIDigFltrMinPulseWidth(taskHandleSOT, "Dev1/port0/line0:3", 0.00001024);//0.00001024
	status |= DAQmxSetDIDigFltrEnable(taskHandleSOT, "Dev1/port0/line0:3", 1);

	status |= DAQmxCreateDOChan(taskHandleInitial, "Dev1/port0/line0:3", "", DAQmx_Val_ChanForAllLines);
	//status |= DAQmxCreateDIChan(taskHandleSOT, "Dev1/port0/line0:3", "", DAQmx_Val_ChanForAllLines);
	status |= DAQmxCreateDOChan(taskHandleEOT0, "Dev1/port1:2", "", DAQmx_Val_ChanForAllLines);
	status |= DAQmxCreateDOChan(taskHandleEOT1, "Dev1/port3:4", "", DAQmx_Val_ChanForAllLines);
	status |= DAQmxCreateDOChan(taskHandleEOT2, "Dev1/port7:8", "", DAQmx_Val_ChanForAllLines);
	status |= DAQmxCreateDOChan(taskHandleEOT3, "Dev1/port9:10", "", DAQmx_Val_ChanForAllLines);

	if (status == 0)
	{
		SOT = CreateEventA(NULL, FALSE, FALSE, "SOTSignal");
		EOT = CreateEventA(NULL, FALSE, TRUE, NULL);
		if (SOT)
		{
			getHandler = true;
			return 0;
		}
		else
		{
			getHandler = false;
			return -1;
		}
	}
	else
	{
		return status;
	}

}

extern "C" HANDLER_API int Start()
{
	int status = 0;
	//SotMonitor
	if (getHandler)
	{	

		//status |= DAQmxStartTask(taskHandleSOT);
		//status |= DAQmxStartTask(taskHandleInitial);
		status |= DAQmxStartTask(taskHandleEOT0);
		status |= DAQmxStartTask(taskHandleEOT1);
		status |= DAQmxStartTask(taskHandleEOT2);
		status |= DAQmxStartTask(taskHandleEOT3);
		SetEvent(EOT);
		WaitingForSOT = TRUE;
		SOTThread = CreateThread(NULL, 0, SOTMonitor, NULL, 0, NULL);
		return 0;
	}
	else
	{
		return status;
	}
}

extern "C" HANDLER_API int Stop()
{
	int status = -1;
	int32		written;
	//SotMonitor
	WaitingForSOT = FALSE;
	DataReset[0] = DataReset[1] = DataReset[2] = DataReset[3] = 0xFFFF;
	status = DAQmxWriteDigitalU16(taskHandleEOT0, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[0], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT1, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[1], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT2, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[2], &written, NULL);
	status = DAQmxWriteDigitalU16(taskHandleEOT3, 1, 1, 1.0, DAQmx_Val_GroupByChannel, &DataReset[3], &written, NULL);

	//status = DAQmxStopTask(taskHandleInitial);
	status = DAQmxStopTask(taskHandleEOT0);
	status = DAQmxStopTask(taskHandleEOT1);
	status = DAQmxStopTask(taskHandleEOT2);
	status = DAQmxStopTask(taskHandleEOT3);


	return 0;
}



extern "C" HANDLER_API int Reset()
{
	int status = -1;
	getHandler = false;
	
	status = DAQmxClearTask(taskHandleSOT);
	status = DAQmxClearTask(taskHandleInitial);
	status = DAQmxClearTask(taskHandleEOT0);
	status = DAQmxClearTask(taskHandleEOT1);
	status = DAQmxClearTask(taskHandleEOT2);
	status = DAQmxClearTask(taskHandleEOT3);

	return status;
}

int IsSiteActive()
{
	int SiteActive = 0;
	//BOOL active = 1 << (SiteNumber - 1) & ActivateSite;
	for (int i = 0;i < 4;i++)
	{	
		if (GetActiveSite[i])
		{
			SiteActive += pow(2, i);
		}
	}	
	return SiteActive;
}


int main()
{
	int site = 0;	
	Setup();
	Start();
	for (int i = 0; i < 100; i++)
	{
		WaitForSingleObject(SOT, INFINITE);
		site = IsSiteActive();
		EOTProcess(hardBins);
	}
	
    return 0;
}

