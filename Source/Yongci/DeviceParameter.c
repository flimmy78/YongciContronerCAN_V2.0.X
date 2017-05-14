/***********************************************
*Copyright(c) 2016,FreeGo
*保留所有权利
*文件名称:Main.c
*文件标识:
*创建日期： 2016年12月23日 
*摘要:

*当前版本:1.0
*作者: FreeGo
*取代版本:
*作者:
*完成时间:
************************************************************/
#include "../Header.h"
#include "xc.h"
#include "../Driver/AdcSample.h"
#include "DeviceParameter.h"
#include "../SerialPort/RefParameter.h"
/********************************************
*函数名：  GetCapVoltage()
*形参：void
*返回值：uint16--电容电压ADC值
*功能：软件启动转换，获取ADC值.
**********************************************/
void GetCapVoltage(void)
{
    SoftSampleOnce();
    g_SystemVoltageParameter.workVoltage = ADCBUF0 * ADC_MODULUS;
    g_SystemVoltageParameter.voltageCap1 = ADCBUF1 * LOCAL_CAP_MODULUS;
    g_SystemVoltageParameter.voltageCap2 = ADCBUF2 * LOCAL_CAP_MODULUS;
    VOLTAGE_CAP3(); //电容3赋值函数
    ClrWdt();
    ClrWdt();
}

/********************************************
*函数名：  GetCapVolatageState()
*形参：void
*返回值：uint16 --电压状态，大于最小值为0xAAAA
*功能：获取电压状态.
**********************************************/
uint16 GetCapVolatageState(void)
{
    GetCapVoltage();
    if ((g_SystemVoltageParameter.voltageCap1  >= LOW_VOLTAGE_ADC) && 
        (g_SystemVoltageParameter.voltageCap2  >= LOW_VOLTAGE_ADC) && 
        (g_SystemVoltageParameter.voltageCap3  >= LOW_VOLTAGE_ADC))
    {
        ClrWdt();
        return 0xAAAA;
    }
    else
    {
        return 0;
    }
}

/**
 * 
 * <p>Function name: [CheckVoltage]</p>
 * <p>Discription: [检测电压的状态,并更新指示灯和继电器]</p>
 */
void CheckVoltage(void)
{
    GetCapVoltage();
    ClrWdt();
    //机构3电容状态更新
    if (g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.upper)
    {
        UpdateIndicateState(CAP1_RELAY , CAP1_LED ,TURN_ON);
    }
    else if(g_SystemVoltageParameter.voltageCap1  >= g_SystemLimit.capVoltage1.down)
    {
        UpdateIndicateState(CAP1_RELAY , CAP1_LED ,TURN_OFF);        
    }
    
    //机构3电容状态更新
    if (g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.upper)
    {
        UpdateIndicateState(CAP2_RELAY , CAP2_LED ,TURN_ON);
    }
    else if(g_SystemVoltageParameter.voltageCap2  >= g_SystemLimit.capVoltage2.down)
    {
        UpdateIndicateState(CAP2_RELAY , CAP2_LED ,TURN_OFF);        
    }
    
    if(CAP3_STATE)
    {
        //机构3电容状态更新
        if (g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.upper)
        {
            UpdateIndicateState(CAP3_RELAY , CAP3_LED ,TURN_ON);
        }
        else if(g_SystemVoltageParameter.voltageCap3  >= g_SystemLimit.capVoltage3.down)
        {
            UpdateIndicateState(CAP3_RELAY , CAP3_LED ,TURN_OFF);        
        }
    }
}
