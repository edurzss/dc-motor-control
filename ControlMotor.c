#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <wiringPi.h>
#include <softPwm.h>

#define ENCODER_PPR 400 //Pulsos por revolucion del encoder de los motores
#define T_TIMER 20 //Periodo de las senales del timer

//------------------Declaracion de Funciones
void *motor1();
void *motor2();
void *encoder1();
void *encoder2();
void *timer_encoder();
void *gestor_operador();

void encoder1_handler();
void encoder2_handler();
void gestor_handler();
float getMotorSpeed(int, int);
float getMotorPos(int);

//------------------Variables y Recursos Compartidos
float speed_m1;	//Velocidad de motor 1 (RPM)
float speed_m2;
float pos_m1; 	//Posicion de motor 1 (angulo en grados sexagesimales)
float pos_m2;

float velocidad_deseada = 900; //Velocidad deseada en RPM (Senal de referencia)

int id_encoder1; 	//Variable para almacenar id del hilo encoder 1
int id_gestor_operador;	//Variable para almacenar id del hilo gestor_operador

int cond_timer1;
int cond_timer2;
int cond_sigint;

pthread_t thread_m1, thread_m2; //Threads para control de motores
pthread_t thread_e1, thread_e2; //Threads para decodificar velocidad de motores
pthread_t thread_timer;
pthread_t thread_gestor; //Thread para interfaz con operador

//------------------Creacion de Hilo Principal
int main()
{
	//Inicializacion de wiringPi y de pines
	wiringPiSetupGpio();	//Usa numeracion de pines Broadcom (GPIO XX)
	
	//Declaracion de pines de entrada
	pinMode(24, INPUT);	//Entrada para Canal A de Encoder de Motor 1
	pinMode(25, INPUT);	//Entrada para Canal B de Encoder de Motor 1
	pinMode(12, INPUT);	//Entrada para Canal A de Encoder de Motor 2
	pinMode(16, INPUT);	//Entrada para Canal B de Encoder de Motor 2

	//Declaracion de pines de salida
	softPwmCreate (23, 0, 100);	//Configura pin como salida PWM(software) para Motor 1
	pinMode (18, PWM_OUTPUT);	//Configura pin como salida PWM para Motor 2
	
	printf("\n Hola, voy a controlar 2 MOTORES DC al mismo tiempo \n");

	pthread_create(&thread_e1, NULL, encoder1, NULL);
	pthread_create(&thread_e2, NULL, encoder2, NULL);

	pthread_create(&thread_timer, NULL, timer_encoder, NULL);
	pthread_create(&thread_gestor, NULL, timer_encoder, NULL);

	printf("main : En espera... \n");


	pthread_join(thread_e1, NULL);
	pthread_join(thread_e2, NULL);
        pthread_join(thread_m1, NULL);
	pthread_join(thread_m2, NULL);
	pthread_exit(NULL);
}
//-----------------Hilos para motores
void *motor1()
{
	int i;
	int DC_100 = 0;		//Duty Cycle 0 a 100 (Senal de control)
	float Kp = 0.01;	//Constante de control proporcional
	float Ki = 0.001;	//Constante de control integral
	float Kd = 0;		//Constante de control derivativo
	float sum_error=0;	//Sumatoria de los errores
	float pend_error=0;	//Derivada del error
	float speed_error, last_speed_error = 0;	//Error velocidad actual y anterior

	sleep(1); //Espera 1 segundo antes de iniciar control
		
	while(1)
	{
	//Algoritmo de Control de Velocidad
		speed_error = velocidad_deseada - speed_m1;

		//Control Proporcional
		DC_100 = Kp * speed_error;	
		
		//Control Integral
		sum_error += speed_error;
		DC_100 += Ki * sum_error;

		//Control Derivativo
		pend_error = speed_error - last_speed_error;
		DC_100 += Kd * pend_error;

		//Ajuste para contener dentro del Rango	
		if (DC_100 > 100) DC_100 = 100;
		else if (DC_100 < 0) DC_100 = 0;

		softPwmWrite(23, DC_100);

		last_speed_error = speed_error;
		
		delay(10);
		
		
	}
}
//----
void *motor2()
{
	int i;
	int DC_1024 = 0;	//Duty Cycle 0 a 1024 (Senal de control)
	float Kp = 0.1;		//Constante de control proporcional
	float Ki = 0.004;	//Constante de control integrativo
	float Kd = 0;		//Constante de control derivativo
	float sum_error=0;	//Sumatoria de los errores
	float pend_error=0;	//Derivada del error
	float pos_error = 0, last_pos_error = 0;	//Error posicion actual y anterior

	sleep(1); //Espera 1 segundo antes de iniciar control
		
	while(1)
	{
	//Algoritmo de Control de Posicion
		//pos_error = pos_m1 - pos_m2;
		pos_error = velocidad_deseada - speed_m2;
		//Control Proporcional
		DC_1024 = Kp * pos_error;	
		
		//Control Integrativo
		sum_error += pos_error;
		DC_1024 += Ki * sum_error;
		
		//Control Derivativo
		pend_error = pos_error - last_pos_error; 
		DC_1024 += Kd * pend_error;

		//Ajuste para contener dentro del Rango	
		if (DC_1024 > 1024) DC_1024 = 1024;
		else if (DC_1024 < 0) DC_1024 = 0;

		pwmWrite(18, DC_1024);

		last_pos_error = pos_error;
		
		delay(2);
		
		
	}
}
//--------------------Hilos para encoders
void *encoder1()
{
	int level_A, last_level_A; //Nivel y anterior nivel leido (HIGH o LOW)
	int level_B, last_level_B; //
	int pulse_num, last_pulse_num, pulse_dif; //Numero de pulsos;
	
	id_encoder1 = pthread_self();
	signal(SIGUSR1, encoder1_handler); //Asocia hilo con senal SIGUSR1
	pos_m1 = 0;

//Inicializa variables locales
	pulse_num = 0;
	last_pulse_num = 0;

	printf("Encoder 1 : Hola, me acaban de crear \n");

//Inicializa variables de nivel de Canal A y B
	level_A = digitalRead(24);
	level_B = digitalRead(25);

	if (!level_A) last_level_A = LOW;
	else last_level_A = HIGH;

	if (!level_B) last_level_B = LOW;
	else last_level_B = HIGH;

	while(1)
	{	
		level_A = digitalRead(24);
		level_B = digitalRead(25);
		
		if (level_A!=last_level_A) //Verifica si hay cambio de nivel en el Canal A
		{
			if (level_A == HIGH)		//Logica de encoders
			{
				if (!level_B) pulse_num++;
				else pulse_num--;
			}else if (level_A == LOW)
			{
				if (level_B) pulse_num++;
				else pulse_num--;
			}
			last_level_A = level_A;
			pos_m1 = (360.0/(float)ENCODER_PPR)*(float)pulse_num;   //Actualiza variable de posicion de motor 1
		}else if (level_B!=last_level_B) //Verifica si hay cambio de nivel en canal B
		{
			if (level_B == HIGH)		//Logica de encoders
			{
				if (level_A) pulse_num++;
				else pulse_num--;
			}else if (level_B ==LOW)
			{
				if (!level_A) pulse_num++;
				else pulse_num--;
			}
			last_level_B = level_B;
			pos_m1 = (360.0/(float)ENCODER_PPR)*(float)pulse_num;   //Actualiza variable de posicion de motor 1
		}

		/*if (pulse_num == ENCODER_PPR) //Verifica si se llega al maximo de pulsos
		{
			pulse_num = pulse_num - ENCODER_PPR;
			last_pulse_num = last_pulse_num - ENCODER_PPR;
		}*/
		
		if(cond_timer1) //Si recibe senal de timer
		{
			pulse_dif = pulse_num - last_pulse_num;
			speed_m1 = getMotorSpeed(T_TIMER, pulse_dif);
			cond_timer1 = 0;
			last_pulse_num = pulse_num;
		}
	}
}
//----
void *encoder2()
{

	int level_A, last_level_A; //Nivel y anterior nivel leido (HIGH o LOW)
	int level_B, last_level_B; //
	int pulse_num, last_pulse_num, pulse_dif; //Numero de pulsos;
	
	id_encoder2 = pthread_self();
	signal(SIGUSR1, encoder2_handler); //Asocia hilo con senal SIGUSR1
	pos_m2 = 0;

//Inicializa variables locales
	pulse_num = 0;
	last_pulse_num = 0;

	printf("Encoder 2 : Hola, me acaban de crear \n");

//Inicializa variables de nivel de Canal A y B
	level_A = digitalRead(12);
	level_B = digitalRead(16);

	if (!level_A) last_level_A = LOW;
	else last_level_A = HIGH;

	if (!level_B) last_level_B = LOW;
	else last_level_B = HIGH;

	while(1)
	{	
		level_A = digitalRead(12);
		level_B = digitalRead(16);
		
		if (level_A!=last_level_A) //Verifica si hay cambio de nivel en el Canal A
		{
			if (level_A == HIGH)		//Logica de encoders
			{
				if (!level_B) pulse_num++;
				else pulse_num--;
			}else if (level_A == LOW)
			{
				if (level_B) pulse_num++;
				else pulse_num--;
			}
			last_level_A = level_A;
			pos_m2 = (360.0/(float)ENCODER_PPR)*(float)pulse_num;   //Actualiza variable de posicion de motor 1
		}else if (level_B!=last_level_B) //Verifica si hay cambio de nivel en canal B
		{
			if (level_B == HIGH)		//Logica de encoders
			{
				if (level_A) pulse_num++;
				else pulse_num--;
			}else if (level_B ==LOW)
			{
				if (!level_A) pulse_num++;
				else pulse_num--;
			}
			last_level_B = level_B;
			pos_m2 = (360.0/(float)ENCODER_PPR)*(float)pulse_num;   //Actualiza variable de posicion de motor 1
		}

		/*if (pulse_num == ENCODER_PPR) //Verifica si se llega al maximo de pulsos
		{
			pulse_num = pulse_num - ENCODER_PPR;
			last_pulse_num = last_pulse_num - ENCODER_PPR;
		}*/
		
		if(cond_timer2) //Si recibe senal de timer
		{
			pulse_dif = pulse_num - last_pulse_num;
			speed_m2 = getMotorSpeed(T_TIMER, pulse_dif);
			cond_timer2 = 0;
			last_pulse_num = pulse_num;
		}
	}
}
//--------------------Hilo de timer que da periodo para calcular la velocidad
void *timer_encoder()
{
	unsigned int time_now, time_last;
	time_last = 0;
	while(1)
	{
		time_now = millis();
		if (time_now - time_last> T_TIMER)
		{
			pthread_kill(id_encoder1, SIGUSR1);
			time_last = time_now;
		}
	}
}
//--------------------Hilo que interactua con el operador (terminal)
void *gestor_operador()
{	
	//id_gestor_operador = pthread_self();
	signal(SIGINT, gestor_handler); //Asocia hilo con senal SIGINT
	while(1)
	{
		if (cond_sigint) //Si recibe señal "Ctrl+C"
		{
		velocidad_deseada = 0;
		sleep(1);
		pthread_cancel(thread_m1);		
		pthread_cancel(thread_m2);

		printf("\nPor favor, ingrese la velocidad deseada en RPM: ");
		scanf("%d", velocidad_deseada);
		printf("\n");

		pthread_create(&thread_m1,NULL, motor1, NULL);
		pthread_create(&thread_m2, NULL, motor2, NULL);

		cond_sigint = 0;
		}
		printf("Pos M1: %0.1f grados - Vel M1: %0.1f RPM\tPos M2: %0.1f grados - Vel M2: %0.1f RPM \r", pos_m1, speed_m1, pos_m2, speed_m2);
	}
}
//---- Otras funciones
float getMotorSpeed(int time_lapse, int pulses)
{	
	float speed = 0;
	speed = ((float)pulses/(float)time_lapse) * (1000 * 60)/ENCODER_PPR;
	return (speed);
}
float getMotorPos(int pulses)
{
	float pos = 0;
	pos = pulses;
	return (pos);
}

//--- Manejadores
void encoder1_handler()
{
	cond_timer1 = 1;
}
void encoder2_handler()
{
	cond_timer2 = 1;
}
void gestor_handler()
{
	cond_sigint = 1;
}
