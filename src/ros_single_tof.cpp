#include "VL53L0X.h"

#include <chrono>
#include <csignal>
#include <exception>
#include <iomanip>
#include <iostream>
#include <unistd.h>

#include <ros/ros.h>
#include "std_msgs/Int32.h"

// SIGINT (CTRL-C) exit flag and signal handler
volatile sig_atomic_t exitFlag = 0;
void sigintHandler(int) {
	exitFlag = 1;
}

// Uncomment to enable long range measurements
// #define LONG_RANGE
// Uncomment ONE to enable high speed or high accuracy measurements
// #define HIGH_SPEED
// #define HIGH_ACCURACY
#ifdef HIGH_SPEED
	#ifdef HIGH_ACCURACY
		#error HIGH_SPEED and HIGH_ACCURACY cannot be both enabled at once!
	#endif
#endif

int main(int argc, char** argv) {
	// Register SIGINT handler
	signal(SIGINT, sigintHandler);

	// Create the sensor with default values
	VL53L0X sensor;
	try {
		// Initialize the sensor
		sensor.initialize();
		// Set measurement timeout value
		sensor.setTimeout(200);
	} catch (const std::exception & error) {
		std::cerr << "Error initializing sensor with reason:" << std::endl << error.what() << std::endl;
		return 1;
	}

	#ifdef LONG_RANGE
		try {
			// Lower the return signal rate limit (default is 0.25 MCPS)
			sensor.setSignalRateLimit(0.1);
			// Increase laser pulse periods (defaults are 14 and 10 PCLKs)
			sensor.setVcselPulsePeriod(VcselPeriodPreRange, 18);
			sensor.setVcselPulsePeriod(VcselPeriodFinalRange, 14);
		} catch (const std::exception & error) {
			std::cerr << "Error enabling long range mode with reason:" << std::endl << error.what() << std::endl;
			return 2;
		}
	#endif

	#if defined HIGH_SPEED
		try {
			// Reduce timing budget to 20 ms (default is about 33 ms)
			sensor.setMeasurementTimingBudget(20000);
		} catch (const std::exception & error) {
			std::cerr << "Error enabling high speed mode with reason:" << std::endl << error.what() << std::endl;
			return 3;
		}
	#elif defined HIGH_ACCURACY
		try {
			// Increase timing budget to 200 ms
			sensor.setMeasurementTimingBudget(200000);
		} catch (const std::exception & error) {
			std::cerr << "Error enabling high accuracy mode with reason:" << std::endl << error.what() << std::endl;
			return 3;
		}
	#endif

	// Highly unprobable but check SIGINT exit flag
	if (exitFlag) {
		return 0;
	}

	// Also, set width/fill for cout stream so that measurements are aligned
	std::cout << "\rReading" << std::setw(4) << std::setfill('0');
	// Take the measurements!
  ros::init(argc, argv, "single_tof");
  ros::NodeHandle nh;
  ros::Publisher tof_range_pub = nh.advertise<std_msgs::Int32>("/tof_range", 10);
  ros::Rate rate(10);
	while (ros::ok()) 
	{
		uint16_t distance;
		std_msgs::Int32 range_msg;
		try 
		{
			// Read the range. Note that it's a blocking call
			distance = sensor.readRangeSingleMillimeters();
		} catch (const std::exception & error) 
		{
			std::cerr << "Error getting measurement with reason:" << std::endl << error.what() << std::endl;
			// You may want to bail out here, depending on your application - error means issues on I2C bus read/write.
			// return 3;
			distance = 8096;
		}

		// Check IO timeout and print range information
		if (sensor.timeoutOccurred()) 
		{
			std::cout << "\rToF Reading timeout occured!" << std::endl;
		} 
		else 
		{
			range_msg.data = distance;
			tof_range_pub.publish(range_msg);
		}

		rate.sleep();
	}

	return 0;
}
