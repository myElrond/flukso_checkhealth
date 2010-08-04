//============================================================================
// Name        : flukso-checkhealth.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <string.h>
#include <vector>
#include <curl/curl.h>
#include <json/json.h>
#include <stdlib.h>
#include <popt.h>

using namespace std;

static vector<char*> receivedData;
static vector<int> keyData;
static vector<int> valueData;

enum opMode
{
	MODE_WATCHDOG = 0, MODE_LAST_VALID_LOAD = 1, MODE_WATCHDOG_NAGIOS = 2, MODE_VALID_LOAD_NAGIOS = 3
};

static struct
{
	char* token;
	char* sensor;
	char* baseURL;
	char* unit;
	char* interval;
	int debug;
	long int maxAge;
	long int criticalTimeDiff;
	long int warningTimeDiff;
	int mode;
	bool verbose;
} configData;

void dump_configdata()
{
	cout << "Dumping configdata struct" << endl;
	if (!configData.token)
		cout << "Token: NULL" << endl;
	else
		cout << "Token: " << configData.token;

	if (!configData.sensor)
		cout << "Sensor: NULL" << endl;
	else
		cout << "Sensor: " << configData.sensor;

	if (!configData.baseURL)
		cout << "BaseURL: NULL" << endl;
	else
		cout << "BaseURL: " << configData.baseURL;

	if (!configData.unit)
		cout << "Unit: NULL" << endl;
	else
		cout << "Unit: " << configData.unit;

	if (!configData.interval)
		cout << "Interval: NULL" << endl;
	else
		cout << "Interval: " << configData.interval << endl;

	cout << "MaxTimeDiff: " << configData.maxAge << endl;
	cout << "CriticalTimeDiff: " << configData.criticalTimeDiff << endl;
	cout << "WarningTimeDiff: " << configData.warningTimeDiff << endl;
	cout << "Mode: " << configData.mode << endl;
	if (configData.verbose)
		cout << "Verbose: TRUE" << endl;
	else
		cout << "Verbose: FALSE" << endl;
}

// Receiving Data from CURL
size_t getMyCURLData(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t realsize = size * nmemb;

	if (realsize > 0)
	{
		char* returndata = new char[realsize];
		memcpy(returndata, ptr, realsize);
		receivedData.push_back(returndata);

	}

	return realsize;
}

int process_data()
{

	if (configData.debug)
		cout << "Starting processing for mode " << configData.mode << "..." << endl;

	time_t currentTime;
	time_t lastValidTime;
	currentTime = time(NULL);

	// Checking for last valid value
	for (int i = keyData.size() - 1; i >= 0; i--)
	{
		if (valueData.at(i) >= 0)
		{
			lastValidTime = keyData.at(i);
			if (configData.debug)
			{
				cout << "Last valid key  : " << keyData.at(i) << endl;
				cout << "Current time    : " << currentTime << endl;
				cout << "Last valid time : " << lastValidTime << endl;
			}

			// Found a value. Check mode

			if (configData.mode == MODE_WATCHDOG)
			{
				if (abs(currentTime - lastValidTime) > configData.maxAge)
				{
					if (configData.verbose)
						cout << "Last valid reading was seen " << abs(currentTime - lastValidTime) << " seconds ago and is "
								<< abs(currentTime - lastValidTime) - configData.maxAge << " seconds older than the age." << endl;
					else
						cout << abs(currentTime - lastValidTime) << endl;
					return 1;
				}
				else
				{
					if (configData.verbose)
						cout << "Last valid value is " << abs(currentTime - lastValidTime) << " seconds old." << endl;
					else
						cout << abs(currentTime - lastValidTime) << endl;
				}
				break;
			}

			if (configData.mode == MODE_LAST_VALID_LOAD)
			{
				if ((configData.maxAge > 0) && (abs(currentTime - lastValidTime) > configData.maxAge))
				{
					if (configData.verbose)
						cout << "Last valid reading was " << valueData.at(i) << " " << configData.unit << " and is " << abs(
								currentTime - lastValidTime) - configData.maxAge << " seconds older than the required age of "
								<< configData.maxAge << " seconds." << endl;
					else
						cout << "Value outdated." << endl;

					return 2;
				}
				else if (configData.verbose)
					cout << "Last valid reading was " << valueData.at(i) << " " << configData.unit << " and is " << abs(
							currentTime - lastValidTime) << " seconds old." << endl;
				else
					cout << valueData.at(i) << endl;
				break;

			}
		}
	}

	// In all other cases
	return 0;

}

int parse_json_data(char* inputData)
{

	struct json_object *myJSONobj;
	struct json_object *myCurrent;
	struct json_object *myCurrentElement;
	unsigned int length;

	myJSONobj = json_tokener_parse(inputData);

	length = json_object_array_length(myJSONobj);

	if (configData.debug)
		cout << "Data length: " << length << " bytes" << endl;

	// Continue if we have elements
	if (length > 0)

	{
		// Walk through all entries
		for (unsigned int i = 0; i < length; i++)
		{
			// Get an entries
			myCurrent = json_object_array_get_idx(myJSONobj, i);

			// Is the current entry an array?
			if (json_object_get_type(myCurrent) != json_type_array)
			{

				cerr << "Error decoding JSON data. Aborting." << endl;
				return 1;
			}

			// Has it two elements?
			if (json_object_array_length(myCurrent) != 2)
			{
				cerr << "Array element malformed. Expected two elements but got " << json_object_array_length(myCurrent) << ": "
						<< json_object_get_string(myCurrent) << endl;
				continue;
			}

			myCurrentElement = json_object_array_get_idx(myCurrent, 0);
			if (json_object_get_type(myCurrentElement) != json_type_int)
			{
				cerr << "First array element is not an integer at entry" << i << ": " << json_object_get_string(myCurrent);
				keyData.push_back(-1);
				continue;
			}

			keyData.push_back(json_object_get_int(myCurrentElement));

			myCurrentElement = json_object_array_get_idx(myCurrent, 1);
			if (json_object_get_type(myCurrentElement) == json_type_int)
			{
				valueData.push_back(json_object_get_int(myCurrentElement));

			}
			else if (json_object_get_type(myCurrentElement) == json_type_string)
			{
				valueData.push_back(-1);
			}
			else
			{
				cerr << "Second array element is neither string or integer at entry" << i << ": " << json_object_get_string(
						myCurrent);
				valueData.push_back(-1);
				continue;
			}

		}
	}

	else
	{
		cerr << "Tokenizing JSON response failed. Aborting." << endl;
		return 1;
	}

	return 0;
}

CURLcode do_curl()
{
	CURL *curl;
	CURLcode res;

	struct curl_slist *slist = NULL;

	curl = curl_easy_init();

	if (curl)
	{
		string urlToCall = string(configData.baseURL) + string(configData.sensor) + "?interval=" + string(configData.interval)
				+ "&unit=" + string(configData.unit);

		if (configData.debug)
			cout << "urlToCall: " << urlToCall << endl;

		curl_easy_setopt(curl, CURLOPT_URL, urlToCall.c_str());
	}

	// Disable Peer Verification
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	// Adding Headers
	slist = curl_slist_append(slist, "Accept: application/json");
	slist = curl_slist_append(slist, "X-Version: 1.0");
	string tokenHeader = "X-Token: " + string(configData.token);
	slist = curl_slist_append(slist, tokenHeader.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

	// Forward received data to own function
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, getMyCURLData);

	// Performing request
	if (configData.debug)
		cout << "Connecting to server and fetching data ...";
	res = curl_easy_perform(curl);
	if (configData.debug)
		cout << "finished." << endl;

	// Cleaning up
	curl_easy_cleanup(curl);
	curl_slist_free_all(slist);

	return res;
}

void do_cleanup(poptContext optCon)
{
	for (unsigned int i = 0; i < receivedData.size(); i++)
		delete receivedData.at(i);

	receivedData.clear();
	keyData.clear();
	valueData.clear();
	poptFreeContext(optCon);
}

void print_usage(poptContext optCon, int exitcode)
{
	poptPrintHelp(optCon, stderr, 0);
	// if (error)
	// 	cerr << "Error parsing commandline: " << error << ": " << addl << endl;
	do_cleanup(optCon);
	exit(exitcode);
}

void check_arguments(poptContext optCon)
{

	char* charHelper;
	string stringHelper;


	// Check cmdline arguments for plausibility

	if ((configData.token == NULL) || (configData.sensor == NULL))
	{
		cerr << endl << "Error: Sensor and Token values MUST be provided for proper function." << endl << endl;
		print_usage(optCon, 5);
	}

	if ((configData.mode != MODE_WATCHDOG) && (configData.mode != MODE_LAST_VALID_LOAD) && (configData.mode
			!= MODE_WATCHDOG_NAGIOS) && (configData.mode != MODE_VALID_LOAD_NAGIOS))
	{
		cout << "Mode omitted or invalid. Setting default mode of 0" << endl;
		configData.mode = 0;
	}

	if (configData.baseURL == NULL)
	{
		if (configData.debug)
			cout << "Base URL omitted. Setting default to https://api.mysmartgrid.de/sensor/" << endl;
		stringHelper = "https://api.mysmartgrid.de/sensor/";
		charHelper = new char[stringHelper.size() + 1];
		strcpy(charHelper, stringHelper.c_str());
		configData.baseURL = charHelper;
	}

	if (configData.unit == NULL)
	{
		if (configData.debug)
			cout << "Unit omitted. Setting default to watt" << endl;
		stringHelper = "watt";
		charHelper = new char[stringHelper.size() + 1];
		strcpy(charHelper, stringHelper.c_str());
		configData.unit = charHelper;
	}

	if (configData.interval == NULL)
	{
		if (configData.debug)
			cout << "Intervall omitted. Setting default to hour" << endl;
		stringHelper = "hour";
		charHelper = new char[stringHelper.size() + 1];
		strcpy(charHelper, stringHelper.c_str());
		configData.interval = charHelper;
	}

}

int main(int argc, const char *argv[])
{

	CURLcode res;
	poptContext optCon;
	int poptRC;
	unsigned int retcode = 0;

	// Define CmdLine Options Context
	struct poptOption cmdLineOpts[] =
	{
			{ "mode", 'm', POPT_ARG_INT, &configData.mode, 0, "0=Watchdog, 1=Last valid load, 2=Watchdog+Nagios, 3=Load+Nagios",
					"mode" },
			{ "debug", 'd', POPT_ARG_NONE, &configData.debug, 0, "Debug mode", NULL },
			{ "token", 't', POPT_ARG_STRING, &configData.token, 0, "SmartGrid token", "string" },
			{ "sensor", 's', POPT_ARG_STRING, &configData.sensor, 0, "SmartGrid sensor", "string" },
			{ "baseURL", 'b', POPT_ARG_STRING, &configData.baseURL, 0, "SmartGrid Base-URL", "url" },
			{ "age", 'a', POPT_ARG_LONG, &configData.maxAge, 0, "Age a value is considered outdated (not in nagios mode)",
					"seconds" },
			{ "critical", 'c', POPT_ARG_LONG, &configData.criticalTimeDiff, 0, "Value raising critical state in nagios mode",
					"seconds" },
			{ "warning", 'w', POPT_ARG_LONG, &configData.warningTimeDiff, 0, "Value raising warning state in nagios mode",
					"seconds" },
			{ "interval", 'i', POPT_ARG_STRING, &configData.interval, 0,
					"Time interval to fetch (hour, day, month, year, night)", "interval" },
			{ "unit", 'u', POPT_ARG_STRING, &configData.unit, 0, "Unit to fetch (watt)", "unit" },
			{ "verbose", 'v', POPT_ARG_NONE, &configData.verbose, 0, "Verbose output", NULL },
			POPT_AUTOHELP POPT_TABLEEND // Needed to terminate array
		};

		// Get CmdLine
		optCon = poptGetContext(NULL, argc, argv, cmdLineOpts, 0);

		// Check arguemnts
		if (argc < 3)
		{
			print_usage(optCon, 0);
			exit(1);
		}

		// Parse arguments
		poptRC = poptGetNextOpt(optCon);

		// Parsing successful?
		if (poptRC < -1)
		{
			cerr << "Commandline Parsing failed: " << poptBadOption(optCon, POPT_BADOPTION_NOALIAS) << ": " <<poptStrerror(poptRC) << endl;
			exit(10);
		}

		if (configData.debug)
		{
			dump_configdata();
		}

		// Check arguments
		check_arguments(optCon);

		// Fetch data
		res = do_curl();

		// Did it work out?
		if (res != 0)
		{
			cerr << "CURL call failed with error-code: " << res << endl;
			do_cleanup(optCon);
			return 1;
		}

		if (receivedData.size() < 1)
		{
			cerr << "CURL call returned no usable data. Please check config (Sensor, Token, Interval, Unit). Aborting" << endl;
			do_cleanup(optCon);
			return 2;
		}

		if (configData.debug)
		cout << "Parsing retrived data ..." << endl;

		for (unsigned int i = 0; i < receivedData.size(); i++)
		if (parse_json_data(receivedData[i]) == 0)
		{
			if (configData.debug)
			cout << "Parsing data was successful." << endl;

			if ( (retcode = process_data()) > 0)
			{
				break;
			}
		}
		else
		{
			cerr << "Parsing json_data failed. Look for error message above. Aborting" << endl;
			retcode = 3;
			break;
		}

		do_cleanup(optCon);
		return retcode;
	}
