// SensorFailureDetector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
using namespace std;
#pragma warning(disable:4244)
#pragma warning(disable:4305)

#define BYTE_TO_DEGREES          1.4117647 // 360/255
#define MIN_VOLTMETER_INACCURACY 1.4117647 // 360/255
#define COUNTS_TO_DEGREES        170.666666 // 2048*30/360
//##################################################################################################################################################
bool ParseCommandParameters(int argc, TCHAR* argv[], basic_string<TCHAR>& sInputFileName)
{
	// we get the file name from the command string
	if(argc == 2)
	{
		sInputFileName = argv[1];
		return true;
	}
	
	return false;
}
//##################################################################################################################################################
int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	basic_string<TCHAR> sInputFileName;
	float fCurrentTime;
	int iCurrentRotation, iCurrentVoltage;
	float fCurrentAngleByEncoder, fCurrentAngleByVoltmeter, fInitialAngle;
	float fDeltaMax, fDeltaCurrent, fDeltaSum;
	int iVoltageSum;
	int iCounter;
	bool bDoItOnce;
	int iMaxVoltmeter, iMinVoltmeter, iMaxEncoder, iMinEncoder;
	float fTrustedStandardDeviation;
	float fRealStandardDeviation;
	float fMaxVoltmeterDeviation, fMaxEncoderDeviation;
	float fThreshold;
	float fSpikeTime;
	bool bWeHadSpike;
	bool bTimeToRemember;
//............................................................................................................................................	
	FILE* InputFile = NULL;
	//............................................................................................................................................	
	// Get input file name
	if (!ParseCommandParameters(argc, argv, sInputFileName))
	{
		wprintf(_T("Cannot recognize program parameter!\nPress any key..."));
		getchar();
		return 0;
	}
	//............................................................................................................................................	
	// Open the file
	_wfopen_s(&InputFile, sInputFileName.c_str(), _T("r"));   
	if (!InputFile)
	{
		wprintf(_T("Cannot open the file: %s\nPress any key..."), sInputFileName.c_str());
		getchar();
		return 0;
	}
	//............................................................................................................................................	
	// initialize values
	fDeltaMax = 0.0;
	fDeltaSum = 0.0;
	iVoltageSum = 0;
	iCounter = 0;
	bDoItOnce = true;
	bWeHadSpike = false;
	bTimeToRemember = true;
	//............................................................................................................................................	

// main loop - read the file:
	while (fscanf_s(InputFile, "%f %ld %d", &fCurrentTime, &iCurrentRotation, &iCurrentVoltage) != EOF)
	{
		fCurrentAngleByVoltmeter = iCurrentVoltage * BYTE_TO_DEGREES; // how many degrees the potentiometer (voltmeter) shows
		
		if (fCurrentTime < 0.001)
		{
			// to find max and min measures of voltmeter and of encoder 
			iMaxVoltmeter = iCurrentVoltage;
			iMinVoltmeter = iCurrentVoltage;
			iMaxEncoder = iCurrentRotation;
			iMinEncoder = iCurrentRotation;

			fInitialAngle = fCurrentAngleByVoltmeter; // assume that the initial angle difference between an Encoder and Voltmeter 
			//is the very first one, use it to calculate standard deviation of measurements difference for the first 0.5 sec,
			// i.e. Trusted Standard Deviation
		}
		
		fCurrentAngleByEncoder = fInitialAngle + iCurrentRotation / COUNTS_TO_DEGREES; // degrees value shown by encoder

		if (fCurrentTime <= 0.500) // calibration time - for first half a second, assume everything is working fine
		{
			++iCounter;
          // Need an average initial angle (for Voltmeter only - assume that an average initial angle of Encoder is 0)
          iVoltageSum = iVoltageSum + iCurrentVoltage;

		  // Find max and min measures of encoder and voltmeter:
		  if (iMaxVoltmeter < iCurrentVoltage)
			  iMaxVoltmeter = iCurrentVoltage;

		  if (iMinVoltmeter > iCurrentVoltage)
			  iMinVoltmeter = iCurrentVoltage;

		  if (iMaxEncoder < iCurrentRotation)
			  iMaxEncoder = iCurrentRotation;

		  if (iMinEncoder > iCurrentRotation)
			  iMinEncoder = iCurrentRotation;

		  // calculate standard deviation of measurements difference for the first 0.5. sec:
		  fDeltaCurrent = fCurrentAngleByVoltmeter - fCurrentAngleByEncoder;
		  fDeltaSum = fDeltaSum + fDeltaCurrent * fDeltaCurrent;
		}
		
		else // if it is later that 0.5 seconds
		{
			if (bDoItOnce) // one time operations:
			{
				bDoItOnce = false;

				// get corrected initial angle:
				fInitialAngle = (iVoltageSum * BYTE_TO_DEGREES) / iCounter; // now it is the average angle

				// trusted standard deviation:
				fTrustedStandardDeviation = sqrt(fDeltaSum / iCounter);
				if (fTrustedStandardDeviation < MIN_VOLTMETER_INACCURACY)
					fTrustedStandardDeviation = MIN_VOLTMETER_INACCURACY; // cannot be less than the inaccuracy of voltmeter measurements

				// find the threshold, i.e. maximal possible difference between encoder and voltmeter measures:
				fMaxVoltmeterDeviation = (iMaxVoltmeter - iMinVoltmeter) * BYTE_TO_DEGREES;
				fMaxEncoderDeviation = (iMaxEncoder - iMinEncoder) / COUNTS_TO_DEGREES;
				fThreshold = fMaxVoltmeterDeviation + fMaxEncoderDeviation;
				if (fThreshold < MIN_VOLTMETER_INACCURACY)
					fThreshold = MIN_VOLTMETER_INACCURACY; // cannot be less than the inaccuracy of voltmeter measurements

				iCounter = 0;
				fDeltaSum = 0.0; // to continue calculating standard deviation after 0.5 seconds
			} //if (bDoItOnce)

			// Now we go further (from 0.5. seconds to the end of the file):
			++iCounter;
			fDeltaCurrent = fCurrentAngleByVoltmeter - fCurrentAngleByEncoder;
			if ((abs(fDeltaCurrent) > fThreshold) && bTimeToRemember)
			{
				bTimeToRemember = false;
				// the difference is more than the threshold - something is already wrong, but maybe it is just an occasional fluke?
				// remember this moment:
				fSpikeTime = fCurrentTime;
				bWeHadSpike = true;
			}
			fDeltaSum = fDeltaSum + fDeltaCurrent * fDeltaCurrent;
		} // later that 0.5 seconds
		
	} // end of the main loop (end of the file)
//...............................................................................................................................................
	fclose(InputFile);

	// After the file is finished

	 // get real standard deviation:
	fRealStandardDeviation = sqrt(fDeltaSum / iCounter);

	if (bWeHadSpike && (fRealStandardDeviation > fTrustedStandardDeviation))
	{
		// the measurements are not acceptable:
		wprintf(_T("The measurements are not acceptable since: %.3f seconds\nPress any key..."), fSpikeTime);
	}
	else
	{
		// the measurements are acceptable:
		wprintf(_T("The measurements are acceptable\nPress any key..."));
	}
	//............................................................................................................................................	

	getchar(); // to see the screen
    return 0;
}

