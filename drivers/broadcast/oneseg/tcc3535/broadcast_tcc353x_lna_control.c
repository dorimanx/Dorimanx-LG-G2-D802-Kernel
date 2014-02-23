
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "tcpal_os.h"
#include "tcc353x_hal.h"
#include "tcc353x_lna_control.h"
#include "tcc353x_user_defines.h"
#include "tcc353x_monitoring.h"

#include <linux/time.h>
#include <linux/ktime.h>

extern TcpalSemaphore_t Tcc353xLnaControlSema;
extern TcpalSemaphore_t Tcc353xDrvSem;

struct workqueue_struct *WorkQueue_Lna_Control;
struct work_struct Work_Lna_Control;
struct timer_list Lna_timer;

#define _MSEC_1000_	(HZ)
#define _MSEC_500_	(HZ/2)
#define _MSEC_250_	(HZ/4)
#define _TIME_OFFSET_ 	_MSEC_500_

I32S TimerStart = 0;
I32S LnaControlStart = 0;
extern Tcc353xStatus_t SignalInfo;
extern I32S OnAir;

#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
static void tcc353x_lnaControl_workQueue(struct work_struct *_param)
{
	Tcc353xStatus_t st;
	I32S ret;

	if(!LnaControlStart) {
		return;
	}

	TcpalSemaphoreLock(&Tcc353xLnaControlSema);
	if(!LnaControlStart) {
		TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);
		return;
	}
	TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);

	TcpalSemaphoreLock(&Tcc353xDrvSem);
	if(OnAir == 0) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return;
	}

	ret = Tcc353xMonitoringApiGetStatus(0, 0, &st);
	if(ret!=TCC353X_RETURN_SUCCESS) {
		TcpalSemaphoreUnLock(&Tcc353xDrvSem);
		return;
	}
	Tcc353xMonitoringApiAntennaPercentage (0, &st, 
					       sizeof(Tcc353xStatus_t));
	TcpalMemcpy(&SignalInfo, &st, sizeof(Tcc353xStatus_t));

#if defined (_USE_LNA_CONTROL_)
	Tcc353xApiLnaControl(0, 0, &st);
#endif
	TcpalSemaphoreUnLock(&Tcc353xDrvSem);
}
#endif

#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
static void tcc353x_timer_function(unsigned long data)
{
	if(TimerStart) {
		Lna_timer.expires = jiffies + _TIME_OFFSET_;
		queue_work(WorkQueue_Lna_Control, &Work_Lna_Control);
		add_timer(&Lna_timer);
	}
}
#endif

void tcc353x_lnaControl_pause(void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
	TcpalSemaphoreLock(&Tcc353xLnaControlSema);
	LnaControlStart = 0;
	TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);
#endif
}

void tcc353x_lnaControl_resume(void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
	TcpalSemaphoreLock(&Tcc353xLnaControlSema);
	LnaControlStart = 1;
	TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);
#endif
}

void tcc353x_timer_start (void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
	TimerStart = 1;
	Lna_timer.expires = jiffies + _TIME_OFFSET_;
	Lna_timer.data = 0;
	Lna_timer.function = &tcc353x_timer_function;
	add_timer(&Lna_timer);
#endif
}

void tcc353x_timer_stop (void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
	TimerStart = 0;
#endif
}

void tcc353x_lnaControl_start(void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
	TcpalSemaphoreLock(&Tcc353xLnaControlSema);
	LnaControlStart = 0;	/* start false */
	TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);

	/* work queue */
	WorkQueue_Lna_Control = create_singlethread_workqueue("tcc353x_work");
	INIT_WORK(&Work_Lna_Control, tcc353x_lnaControl_workQueue);

	/* timer */
	init_timer(&Lna_timer);
#endif
}

void tcc353x_lnaControl_stop(void)
{
#if defined (_USE_LNA_CONTROL_) || defined (_USE_MONITORING_TASK_)
        del_timer_sync(&Lna_timer);

	TcpalSemaphoreLock(&Tcc353xLnaControlSema);
	LnaControlStart = 0;
	TcpalSemaphoreUnLock(&Tcc353xLnaControlSema);

	flush_workqueue(WorkQueue_Lna_Control);
	destroy_workqueue(WorkQueue_Lna_Control);
#endif
}

