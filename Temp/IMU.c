//---------------------------------------------------------------------------------------------------
// Header files
#include "IMU.h"

#define M_PI 3.14159265358979323846
//---------------------------------------------------------------------------------------------------
// Definitions

#define sampleFreq	 200.0f			// sample frequency in Hz
#define twoKpDef	(2.0f * 10.2f)	// 2 * proportional gain 5.2
#define twoKiDef	(2.0f * 0.0f)	// 2 * integral gain

#define betaDef		0.3f		// 2 * proportional gain

#define A   0.594499441455580
#define B   -0.045326765125781
#define C   -0.800494176998940

#define S   -0.076022935353703

//---------------------------------------------------------------------------------------------------
// Variable definitions
 float beta = betaDef;								                // 2 * proportional gain (Kp)
 float twoKp = twoKpDef;											// 2 * proportional gain (Kp)
 float twoKi = twoKiDef;											// 2 * integral gain (Ki)
 float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;					// quaternion of sensor frame relative to auxiliary frame
 float integralFBx = 0.0f,  integralFBy = 0.0f, integralFBz = 0.0f;	// integral error terms scaled by Ki

 float Roll_Last=0.0f;
//---------------------------------------------------------------------------------------------------
// Function declarations

float invSqrt(float x);

//====================================================================================================
// Functions

//---------------------------------------------------------------------------------------------------
// AHRS algorithm update

void MahonyAHRSupdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {  
    float recipNorm;  
    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;    
    float hx, hy, bx, bz;  
    float halfvx, halfvy, halfvz, halfwx, halfwy, halfwz;  
    float halfex, halfey, halfez;  
    float qa, qb, qc;  
  
    gx = gx * M_PI / 180.0f;
    gy = gy * M_PI / 180.0f;
    gz = gz * M_PI / 180.0f;
    
    // Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)  
    if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {  
        MahonyAHRSupdateIMU(gx, gy, gz, ax, ay, az);  
        return;  
    }  
  
    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)  
    if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {  
  
        // Normalise accelerometer measurement  
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);  
        ax *= recipNorm;  
        ay *= recipNorm;  
        az *= recipNorm;       
  
        // Normalise magnetometer measurement  
        recipNorm = invSqrt(mx * mx + my * my + mz * mz);
        mx *= recipNorm;  
        my *= recipNorm;  
        mz *= recipNorm;  

        // Auxiliary variables to avoid repeated arithmetic  
        q0q0 = q0 * q0;  
        q0q1 = q0 * q1;  
        q0q2 = q0 * q2;  
        q0q3 = q0 * q3;  
        q1q1 = q1 * q1;  
        q1q2 = q1 * q2;  
        q1q3 = q1 * q3;  
        q2q2 = q2 * q2;  
        q2q3 = q2 * q3;  
        q3q3 = q3 * q3;     
  
        // Reference direction of Earth's magnetic field  
        hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));  
        hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));  
        bx = sqrt(hx * hx + hy * hy);  
        bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));  
        
        // Estimated direction of gravity and magnetic field  
        halfvx = q1q3 - q0q2;  
        halfvy = q0q1 + q2q3;  
        halfvz = q0q0 - 0.5f + q3q3;  
        halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);  
        halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);  
        halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);    
      
        // Error is sum of cross product between estimated direction and measured direction of field vectors  
        halfex = (ay * halfvz - az * halfvy) + (my * halfwz - mz * halfwy);  
        halfey = (az * halfvx - ax * halfvz) + (mz * halfwx - mx * halfwz);  
        halfez = (ax * halfvy - ay * halfvx) + (mx * halfwy - my * halfwx);   
        // Compute and apply integral feedback if enabled  
        if(twoKi > 0.0f) {  
            integralFBx += twoKi * halfex * (1.0f / sampleFreq);    // integral error scaled by Ki  
            integralFBy += twoKi * halfey * (1.0f / sampleFreq);  
            integralFBz += twoKi * halfez * (1.0f / sampleFreq);  
            gx += integralFBx;  // apply integral feedback  
            gy += integralFBy;  
            gz += integralFBz;  
        }  
        else {  
            integralFBx = 0.0f; // prevent integral windup  
            integralFBy = 0.0f;  
            integralFBz = 0.0f;  
        }  
  
        // Apply proportional feedback  
        gx += twoKp * halfex;  
        gy += twoKp * halfey;  
        gz += twoKp * halfez;  
    }  
      
    // Integrate rate of change of quaternion  
    gx *= (0.5f * (1.0f / sampleFreq));     // pre-multiply common factors  
    gy *= (0.5f * (1.0f / sampleFreq));  
    gz *= (0.5f * (1.0f / sampleFreq));  
    qa = q0;  
    qb = q1;  
    qc = q2;  
    q0 += (-qb * gx - qc * gy - q3 * gz);  
    q1 += (qa * gx + qc * gz - q3 * gy);  
    q2 += (qa * gy - qb * gz + q3 * gx);  
    q3 += (qa * gz + qb * gy - qc * gx);   
      
    // Normalise quaternion  
    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);  
    q0 *= recipNorm;  
    q1 *= recipNorm;  
    q2 *= recipNorm;  
    q3 *= recipNorm;  
} 

//---------------------------------------------------------------------------------------------------
// IMU algorithm update
void MahonyAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az) {
	float recipNorm;
	float halfvx, halfvy, halfvz;
	float halfex, halfey, halfez;
	float qa, qb, qc;
    
    gx = gx * M_PI / 180.0f;
    gy = gy * M_PI / 180.0f;
    gz = gz * M_PI / 180.0f;
	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
	if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

		// Normalise accelerometer measurement
		recipNorm = invSqrt(ax * ax + ay * ay + az * az);
		ax *= recipNorm;
		ay *= recipNorm;
		az *= recipNorm;        

		// Estimated direction of gravity and vector perpendicular to magnetic flux
		halfvx = q1 * q3 - q0 * q2;
		halfvy = q0 * q1 + q2 * q3;
		halfvz = q0 * q0 - 0.5f + q3 * q3;
	
		// Error is sum of cross product between estimated and measured direction of gravity
		halfex = (ay * halfvz - az * halfvy);
		halfey = (az * halfvx - ax * halfvz);
		halfez = (ax * halfvy - ay * halfvx);

		// Compute and apply integral feedback if enabled
		if(twoKi > 0.0f) {
			integralFBx += twoKi * halfex * (1.0f / sampleFreq);	// integral error scaled by Ki
			integralFBy += twoKi * halfey * (1.0f / sampleFreq);
			integralFBz += twoKi * halfez * (1.0f / sampleFreq);
			gx += integralFBx;	// apply integral feedback
			gy += integralFBy;
			gz += integralFBz;
		}
		else {
			integralFBx = 0.0f;	// prevent integral windup
			integralFBy = 0.0f;
			integralFBz = 0.0f;
		}

		// Apply proportional feedback
		gx += twoKp * halfex;
		gy += twoKp * halfey;
		gz += twoKp * halfez;
	}
	
	// Integrate rate of change of quaternion
	gx *= (0.5f * (1.0f / sampleFreq));		// pre-multiply common factors
	gy *= (0.5f * (1.0f / sampleFreq));
	gz *= (0.5f * (1.0f / sampleFreq));
	qa = q0;
	qb = q1;
	qc = q2;
	q0 += (-qb * gx - qc * gy - q3 * gz);
	q1 += (qa * gx + qc * gz - q3 * gy);
	q2 += (qa * gy - qb * gz + q3 * gx);
	q3 += (qa * gz + qb * gy - qc * gx); 
	
	// Normalise quaternion
	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	q0 *= recipNorm;
	q1 *= recipNorm;
	q2 *= recipNorm;
	q3 *= recipNorm;
}

void GetTheEuler(float *Roll,float *Pitch,float *Yaw)
{
  float gx,gy,gz;
  gx = 2*(q1*q3 - q0*q2);
  gy = 2*(q0*q1 + q2*q3);
  gz = q0*q0 - q1*q1 - q2*q2 + q3*q3;
  
  if(gx > 1) gx = 1;
  if(gx < -1) gx = -1;
  
  *Yaw    = atan2(2*(q1*q2 + q0*q3),q0*q0 + q1*q1 - q2*q2 - q3*q3)*180/M_PI;
  *Pitch  = asin(gx)*180/M_PI;
  *Roll   = atan2(gy,gz)*180/M_PI;  
//  if(*Roll < 0)
//  {
//    *Roll  += 360;
//  }
}

float invSqrt(float x) {
	float halfx = 0.5f * x;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i>>1);
	y = *(float*)&i;
	y = y * (1.5f - (halfx * y * y));
	return y;
}
//---------------------------------------------------------------------------------------------------
// IMU algorithm update
void MadgwickAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az) {
	float recipNorm;
	float s0, s1, s2, s3;
	float qDot1, qDot2, qDot3, qDot4;
	float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2 ,_8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    gx = gx * M_PI / 180.0f;
    gy = gy * M_PI / 180.0f;
    gz = gz * M_PI / 180.0f;
    
	// Rate of change of quaternion from gyroscope
	qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
	qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
	qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
	qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
	if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

		// Normalise accelerometer measurement
		recipNorm = invSqrt(ax * ax + ay * ay + az * az);
		ax *= recipNorm;
		ay *= recipNorm;
		az *= recipNorm;   

		// Auxiliary variables to avoid repeated arithmetic
		_2q0 = 2.0f * q0;
		_2q1 = 2.0f * q1;
		_2q2 = 2.0f * q2;
		_2q3 = 2.0f * q3;
		_4q0 = 4.0f * q0;
		_4q1 = 4.0f * q1;
		_4q2 = 4.0f * q2;
		_8q1 = 8.0f * q1;
		_8q2 = 8.0f * q2;
		q0q0 = q0 * q0;
		q1q1 = q1 * q1;
		q2q2 = q2 * q2;
		q3q3 = q3 * q3;

		// Gradient decent algorithm corrective step
		s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
		s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
		s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
		s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
		recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
		s0 *= recipNorm;
		s1 *= recipNorm;
		s2 *= recipNorm;
		s3 *= recipNorm;

		// Apply feedback step
		qDot1 -= beta * s0;
		qDot2 -= beta * s1;
		qDot3 -= beta * s2;
		qDot4 -= beta * s3;
	}

	// Integrate rate of change of quaternion to yield quaternion
	q0 += qDot1 * (1.0f / sampleFreq);
	q1 += qDot2 * (1.0f / sampleFreq);
	q2 += qDot3 * (1.0f / sampleFreq);
	q3 += qDot4 * (1.0f / sampleFreq);

	// Normalise quaternion
	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	q0 *= recipNorm;
	q1 *= recipNorm;
	q2 *= recipNorm;
	q3 *= recipNorm;
}

// AHRS algorithm update
void MadgwickAHRSupdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz) {
	float recipNorm;
	float s0, s1, s2, s3;
	float qDot1, qDot2, qDot3, qDot4;
	float hx, hy;
	float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;

    gx = gx * M_PI / 180.0f;
    gy = gy * M_PI / 180.0f;
    gz = gz * M_PI / 180.0f;
    
	// Use IMU algorithm if magnetometer measurement invalid (avoids NaN in magnetometer normalisation)
	if((mx == 0.0f) && (my == 0.0f) && (mz == 0.0f)) {
		MadgwickAHRSupdateIMU(gx, gy, gz, ax, ay, az);
		return;
	}

	// Rate of change of quaternion from gyroscope
	qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
	qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
	qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
	qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
	if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

		// Normalise accelerometer measurement
		recipNorm = invSqrt(ax * ax + ay * ay + az * az);
		ax *= recipNorm;
		ay *= recipNorm;
		az *= recipNorm;   

		// Normalise magnetometer measurement
		recipNorm = invSqrt(mx * mx + my * my + mz * mz);
		mx *= recipNorm;
		my *= recipNorm;
		mz *= recipNorm;

		// Auxiliary variables to avoid repeated arithmetic
		_2q0mx = 2.0f * q0 * mx;
		_2q0my = 2.0f * q0 * my;
		_2q0mz = 2.0f * q0 * mz;
		_2q1mx = 2.0f * q1 * mx;
		_2q0 = 2.0f * q0;
		_2q1 = 2.0f * q1;
		_2q2 = 2.0f * q2;
		_2q3 = 2.0f * q3;
		_2q0q2 = 2.0f * q0 * q2;
		_2q2q3 = 2.0f * q2 * q3;
		q0q0 = q0 * q0;
		q0q1 = q0 * q1;
		q0q2 = q0 * q2;
		q0q3 = q0 * q3;
		q1q1 = q1 * q1;
		q1q2 = q1 * q2;
		q1q3 = q1 * q3;
		q2q2 = q2 * q2;
		q2q3 = q2 * q3;
		q3q3 = q3 * q3;

		// Reference direction of Earth's magnetic field
		hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
		hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
		_2bx = sqrt(hx * hx + hy * hy);
		_2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
		_4bx = 2.0f * _2bx;
		_4bz = 2.0f * _2bz;

		// Gradient decent algorithm corrective step
		s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) + _2q1 * (2.0f * q0q1 + _2q2q3 - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
		s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) + _2q0 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q1 * (1 - 2.0f * q1q1 - 2.0f * q2q2 - az) + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
		s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) + _2q3 * (2.0f * q0q1 + _2q2q3 - ay) - 4.0f * q2 * (1 - 2.0f * q1q1 - 2.0f * q2q2 - az) + (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
		s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) + _2q2 * (2.0f * q0q1 + _2q2q3 - ay) + (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
		recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
		s0 *= recipNorm;
		s1 *= recipNorm;
		s2 *= recipNorm;
		s3 *= recipNorm;

		// Apply feedback step
		qDot1 -= beta * s0;
		qDot2 -= beta * s1;
		qDot3 -= beta * s2;
		qDot4 -= beta * s3;
	}

	// Integrate rate of change of quaternion to yield quaternion
	q0 += qDot1 * (1.0f / sampleFreq);
	q1 += qDot2 * (1.0f / sampleFreq);
	q2 += qDot3 * (1.0f / sampleFreq);
	q3 += qDot4 * (1.0f / sampleFreq);

	// Normalise quaternion
	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
	q0 *= recipNorm;
	q1 *= recipNorm;
	q2 *= recipNorm;
	q3 *= recipNorm;
}


//zgh updata
//void ZghAHRSupdate(float gx, float gy, float gz, float mx, float my, float mz) {  
//    float recipNorm;  
//    float q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;    
//    float hx, hy, hz, bx, bz;  
//    float halfwx, halfwy, halfwz;  
//    float halfex, halfey, halfez;  
//    float qa, qb, qc;  
//    float roll;
//    //Convert the 
//    gx = gx * M_PI / 180.0f;
//    gy = gy * M_PI / 180.0f;
//    gz = gz * M_PI / 180.0f;
////    printf("%f \r\n",sqrt(mx * mx + my * my + mz * mz)); 
//    // Normalise magnetometer measurement  
//    recipNorm = invSqrt(mx * mx + my * my + mz * mz);
//    
//    mx *= recipNorm;  
//    my *= recipNorm;  
//    mz *= recipNorm;
//    
//    q0q0 = q0 * q0;  
//    q0q1 = q0 * q1;  
//    q0q2 = q0 * q2;  
//    q0q3 = q0 * q3;  
//    q1q1 = q1 * q1;  
//    q1q2 = q1 * q2;  
//    q1q3 = q1 * q3;  
//    q2q2 = q2 * q2;  
//    q2q3 = q2 * q3;  
//    q3q3 = q3 * q3;     
//
//    // Reference direction of Earth's magnetic field          
//    hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));  
//    hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));      
//    bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));     
//    bx = sqrt(hx * hx + hy * hy);
//    
//    recipNorm = invSqrt(bx * bx + bz * bz);   
//    bx *= recipNorm;  
//    bz *= recipNorm;  
//    
////    printf("bx = %f,bz = %f,theta = %f\r\n",bx,bz,atan2(bz,bx)*180.0f/M_PI);
//    
//    // Estimated direction of gravity and magnetic field  
//    halfwx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);  
//    halfwy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);  
//    halfwz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);    
////    roll = atan2(halfwy,halfwz)*180/M_PI;
////    if(roll < 0 )
////    {
////      roll += 360;
////    }
////    printf("roll2 = %f \r\n",roll);
//
//    // Error is sum of cross product between estimated direction and measured direction of field vectors   
//    halfex = (my * halfwz - mz * halfwy);  
//    halfey = (mz * halfwx - mx * halfwz);  
//    halfez = (mx * halfwy - my * halfwx);  
//    // Compute and apply integral feedback if enabled  
//    if(twoKi > 0.0f) {  
//        integralFBx += twoKi * halfex * (1.0f / sampleFreq);    // integral error scaled by Ki  
//        integralFBy += twoKi * halfey * (1.0f / sampleFreq);  
//        integralFBz += twoKi * halfez * (1.0f / sampleFreq);  
//        gx += integralFBx;  // apply integral feedback  
//        gy += integralFBy;  
//        gz += integralFBz;  
//    }  
//    else {  
//        integralFBx = 0.0f; // prevent integral windup  
//        integralFBy = 0.0f;  
//        integralFBz = 0.0f;  
//    }  
//
//    // Apply proportional feedback  
//    gx += twoKp * halfex;  
//    gy += twoKp * halfey;  
//    gz += twoKp * halfez;   
//      
//    // Integrate rate of change of quaternion  
//    gx *= (0.5f * (1.0f / sampleFreq));     // pre-multiply common factors  
//    gy *= (0.5f * (1.0f / sampleFreq));  
//    gz *= (0.5f * (1.0f / sampleFreq));  
//    qa = q0;  
//    qb = q1;  
//    qc = q2;  
//    q0 += (-qb * gx - qc * gy - q3 * gz);  
//    q1 += (qa * gx + qc * gz - q3 * gy);  
//    q2 += (qa * gy - qb * gz + q3 * gx);  
//    q3 += (qa * gz + qb * gy - qc * gx);   
//      
//    // Normalise quaternion  
//    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);  
//    q0 *= recipNorm;  
//    q1 *= recipNorm;  
//    q2 *= recipNorm;  
//    q3 *= recipNorm;  
//} 

//void Madgwick_ZGH_AHRSupdate(float gx, float gy, float gz, float mx, float my, float mz) {
//	float recipNorm;
//	float s0, s1, s2, s3;
//	float qDot1, qDot2, qDot3, qDot4;
//	float hx, hy;
//	float _2q0mx, _2q0my, _2q0mz, _2q1mx, _2bx, _2bz, _4bx, _4bz, _2q0, _2q1, _2q2, _2q3, _2q0q2, _2q2q3, q0q0, q0q1, q0q2, q0q3, q1q1, q1q2, q1q3, q2q2, q2q3, q3q3;
//
//    gx = gx * M_PI / 180.0f;
//    gy = gy * M_PI / 180.0f;
//    gz = gz * M_PI / 180.0f;  
//
//	// Rate of change of quaternion from gyroscope
//	qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
//	qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
//	qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
//	qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);
//
//	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)  
//    // Normalise magnetometer measurement
//    recipNorm = invSqrt(mx * mx + my * my + mz * mz);
//    mx *= recipNorm;
//    my *= recipNorm;
//    mz *= recipNorm;
//
//    // Auxiliary variables to avoid repeated arithmetic
//    _2q0mx = 2.0f * q0 * mx;
//    _2q0my = 2.0f * q0 * my;
//    _2q0mz = 2.0f * q0 * mz;
//    _2q1mx = 2.0f * q1 * mx;
//    _2q0 = 2.0f * q0;
//    _2q1 = 2.0f * q1;
//    _2q2 = 2.0f * q2;
//    _2q3 = 2.0f * q3;
//    _2q0q2 = 2.0f * q0 * q2;
//    _2q2q3 = 2.0f * q2 * q3;
//    q0q0 = q0 * q0;
//    q0q1 = q0 * q1;
//    q0q2 = q0 * q2;
//    q0q3 = q0 * q3;
//    q1q1 = q1 * q1;
//    q1q2 = q1 * q2;
//    q1q3 = q1 * q3;
//    q2q2 = q2 * q2;
//    q2q3 = q2 * q3;
//    q3q3 = q3 * q3;
//
//    // Reference direction of Earth's magnetic field
//    hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
//    hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
//    _2bx = sqrt(hx * hx + hy * hy);
//    _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
//    _4bx = 2.0f * _2bx;
//    _4bz = 2.0f * _2bz;
//
//    // Gradient decent algorithm corrective step
//    s0 = - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
//    s1 =  _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3)  + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
//    s2 = (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
//    s3 = (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
//    recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3); // normalise step magnitude
//    s0 *= recipNorm;
//    s1 *= recipNorm;
//    s2 *= recipNorm;
//    s3 *= recipNorm;
//
//    // Apply feedback step
//    qDot1 -= beta * s0;
//    qDot2 -= beta * s1;
//    qDot3 -= beta * s2;
//    qDot4 -= beta * s3;
//
//
//	// Integrate rate of change of quaternion to yield quaternion
//	q0 += qDot1 * (1.0f / sampleFreq);
//	q1 += qDot2 * (1.0f / sampleFreq);
//	q2 += qDot3 * (1.0f / sampleFreq);
//	q3 += qDot4 * (1.0f / sampleFreq);
//
//	// Normalise quaternion
//	recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
//	q0 *= recipNorm;
//	q1 *= recipNorm;
//	q2 *= recipNorm;
//	q3 *= recipNorm;
//}

void IMU_Gyro_Updata_Mag_Correct(float yaw, float pitch, float mx, float my, float mz)
{
  float recipNorm;   
  float Roll,Yaw,Pitch;
 
  recipNorm = invSqrt(mx * mx + my * my + mz * mz);   
  mx *= recipNorm;  
  my *= recipNorm;  
  mz *= recipNorm;

  Yaw = yaw*M_PI/180;
  Pitch = pitch*M_PI/180;
  
  Roll = (atan2(mz,my) - atan2((A*sin(Pitch) + B*cos(Pitch))*sin(Yaw) + S*cos(Yaw),-A*sin(Pitch) + C*sin(Pitch)))*180/M_PI;
  if(Roll < 0 )
  {
    Roll += 360;
  }
  printf("%f \r\n",Roll);     
}

void IMU_Mag_Updata(float mx, float my, float mz,float *Roll)
{
  float recipNorm;   
 
  recipNorm = invSqrt(mx * mx + my * my + mz * mz);   
  mx *= recipNorm;  
  my *= recipNorm;  
  mz *= recipNorm;
  
  *Roll = atan2(my,mz)*180/M_PI;
  if(*Roll < 0 )
  {
    *Roll += 360;
  }  
}

void QuaternUpdate(float Roll,float Pitch,float Yaw)
{
  float recipNorm;
  float Roll_sin,Roll_cos;
  float Pitch_sin,Pitch_cos;
  float Yaw_sin,Yaw_cos;
  
  Roll  = Roll  * M_PI /180;
  Pitch = Pitch * M_PI /180;
  Yaw   = Yaw   * M_PI /180;
  
  Roll_sin  = sin(Roll/2);
  Roll_cos  = cos(Roll/2); 
  Pitch_sin = sin(Pitch/2);
  Pitch_cos = cos(Pitch/2);
  Yaw_sin   = sin(Yaw/2);
  Yaw_cos   = cos(Yaw/2);
  
  q0 = Roll_cos*Pitch_cos*Yaw_cos + Roll_sin*Pitch_sin*Yaw_sin;
  q1 = Roll_sin*Pitch_cos*Yaw_cos - Roll_cos*Pitch_sin*Yaw_sin;
  q2 = Roll_cos*Pitch_sin*Yaw_cos + Roll_sin*Pitch_cos*Yaw_sin;
  q3 = Roll_cos*Pitch_cos*Yaw_sin - Roll_sin*Pitch_sin*Yaw_cos;
  
  // Normalise quaternion  
  recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);  
  q0 *= recipNorm;  
  q1 *= recipNorm;  
  q2 *= recipNorm;  
  q3 *= recipNorm;  
}

