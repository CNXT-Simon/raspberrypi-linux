// SPDX-License-Identifier: GPL-2.0
//
// cx20709.c -- CX20709 ALSA SoC audio driver
//
// Copyright (C) 2009-2022 Synaptics Incorporated
//
//
// *************************************************************************
// *  Modified Date:  19/05/22
// *  File Version:   5.19.0.0
// *************************************************************************
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/gpio.h>
#include <sound/jack.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include "cx20709.h"

#define CX20709_NUM_SUPPLIES 9
static const char cx20709_supply_names[CX20709_NUM_SUPPLIES] = {
	"VDD",     /* 1.8V digital power supply*/
	"VDDO1",   /* 1.8V or 3.3V for I2C and NVRAM*/
	"VDDO2",   /* 1.8v or 3.3V for PCM interface*/
	"VDDO3",   /* 1.8V or 3.3V for GPIO*/
	"VDDO4",   /* 3.3V for xtal oscillator and reset*/
	"AVDDH",   /* 3.3V power for HP and Class-D digital circuits*/
	"AVDDH_HP",   /* 3.3V power for HP*/
	"AVDD_CLSDL", /* 3.3V or 5V for left channel class-d*/
	"AVDD_CLSDR"  /* 3.3V or 5V for right channel class-d*/
};

#define CX20709_SPI_WRITE_FLAG 0x80

#define NUM_OF_DAI 2
#define DAI_DP1_NAME "cx20709-dp1"
#define DAI_DP2_NAME  "cx20709-dp2"
#define CAPTURE_STREAM_NAME_1   "DP1 Capture"
#define PLAYBACK_STREAM_NAME_1   "DP1 Playback"
#define CAPTURE_STREAM_NAME_2   "DP2 Capture"
#define  PLAYBACK_STREAM_NAME_2   "DP2 Playback"
#define CX20709_REG_MAX 0x1200
#define AUDDRV_VERSION(major0, major1, minor, build) \
       ((major0) << 24 |(major1) << 16 | (minor) << 8 | (build))

#define CX20709_LOADER_TIMEOUT 50 /*50 ms*/
#define CX20709_SW_RESET_TIMEOUT 50 /*50 ms*/
#define CX20709_MEMORY_UPDATE_TIMEOUT  30 /*5 times*/
#define CX20709_MAX_MEM_BUF 0x100

// registers
#define CX20709_PORT1_TX_FRAME	0x05f2
#define CX20709_PORT1_RX_FRAME	0x05f3
#define CX20709_PORT1_TX_SYNC	0x05f4
#define CX20709_PORT1_RX_SYNC	0x05f5
#define CX20709_PORT1_CONTROL2	0x05f6
#define CX20709_CLOCK_DIVIDER	0x0F50
#define CX20709_PORT1_CONTROL	0x0f51
#define CX20709_PORT1_RX_SLOT1	0x0f57
#define CX20709_PORT1_RX_SLOT2	0x0f58
#define CX20709_PORT1_RX_SLOT3	0x0f59
#define CX20709_PORT1_TX_SLOT1	0x0f5A
#define CX20709_PORT1_TX_SLOT2	0x0f5B
#define CX20709_PORT1_TX_SLOT3	0x0f5C
#define CX20709_PORT1_DELAY	0x0F5D
#define CX20709_PORT2_CONTROL	0x0f5e
#define CX20709_PORT2_FRAME	0x0f5f
#define CX20709_PORT2_SYNC	0x0f60
#define CX20709_PORT2_SAMPLE	0x0f61
#define CX20709_PORT2_RX_SLOT1	0x0f62
#define CX20709_PORT2_RX_SLOT2	0x0f63
#define CX20709_PORT2_TX_SLOT1	0x0f65
#define CX20709_PORT2_TX_SLOT2	0x0f66
#define CX20709_ABCODE		0x1000
#define CX20709_FV_LO	        0x1001
#define CX20709_FV_HI	        0x1002
#define CX20709_VV_LO	        0x1003
#define	CX20709_VV_HI		0x1004
#define	CX20709_CHIP		0x1005
#define CX20709_RELEASE		0x1006
#define CX20709_GPIO_CTRL,	0x1007
#define CX20709_GPIO_DIR,	0x1008
#define CX20709_BUTTON_STATUS,	0x1009
#define CX20709_DAC1_GAIN	0x100d
#define CX20709_DAC2_GAIN	0x100e
#define CX20709_CLASSD_GAIN	0x1011
#define CX20709_DAC3_GAIN	0x1012
#define CX20709_ADC1L_GAIN	0X1013
#define CX20709_ADC1R_GAIN	0X1014
#define CX20709_ADC2L_GAIN	0X1015
#define CX20709_ADC2R_GAIN	0X1016
#define CX20709_VOLUME_MUTE	0x1018
#define CX20709_OUTPUT_CONTROL	0x1019
#define CX20709_INPUT_CONTROL	0x101a
#define CX20709_LINE1_GAIN	0x101b
#define CX20709_LINE2_GAIN	0x101c
#define CX20709_LINE3_GAIN	0x101d
#define CX20709_STREAM3_RATE	0X116D
#define CX20709_STREAM3_ROUTE	0X116E
#define CX20709_STREAM4_RATE	0X116F
#define CX20709_STREAM4_ROUTE	0X1170
#define CX20709_STREAM5_RATE	0x1171
#define CX20709_STREAM6_RATE	0x1172
#define CX20709_DSP_ENDABLE	0x117a
#define CX20709_DSP_ENDABLE2	0x117b
#define CX20709_DSP_INIT	0x117c
#define CX20709_DSP_INIT_NEWC	0x117d
#define CX20709_LOWER_POWER	0x117e
#define CX20709_DACIN_SOURCE	0x117f
#define CX20709_DACSUBIN_SOURCE	0x1180
#define CX20709_I2S1OUTIN_SOURCE 0X1181
#define CX20709_USBOUT_SOURCE	0x1183
#define CX20709_MIX0IN0_SOURCE	0x1184
#define CX20709_MIX0IN1_SOURCE	0x1185
#define CX20709_MIX0IN2_SOURCE	0x1186
#define CX20709_MIX0IN3_SOURCE	0x1187
#define CX20709_MIX1IN0_SOURCE	0x1188
#define CX20709_MIX1IN1_SOURCE	0x1189
#define CX20709_MIX1IN2_SOURCE	0x118a
#define CX20709_MIX1IN3_SOURCE	0x118b
#define CX20709_VOICEIN0_SOURCE	0x118c
#define CX20709_I2S1OUTIN_SOURCE 0X118e
#define CX20709_I2S2OUTIN_SOURCE 0X118f
#define CX20709_DMIC_CONTROL	0x1247
#define CX20709_I2S_OPTION	0x1249


enum {
	MEM_TYPE_RAM = 1/* CTL*/,
	MEM_TYPE_SPX = 2,
	MEM_TYPE_EEPROM = 3,
	MEM_TYPE_CPX = 4,
	MEM_TYPE_EEPROM_RESET = 0x8003,
};

#define CX20709_DRIVER_VERSION AUDDRV_VERSION(4, 19, 0 , 1)

#define CX1070X_MAX_REGISTER 0X1300
#define AUDIO_NAME	"cx20709"
#define MAX_REGISTER_NUMBER 0x1250



/* codec private data*/
struct cx20709_priv {
	struct regmap *regmap;
	unsigned int sysclk;
	int is_clk_gated[NUM_OF_DAI];
	int master[NUM_OF_DAI];
	struct device *dev;
	const struct snd_soc_codec_driver *codec_drv;
	struct snd_soc_dai_driver *dai_drv;
	struct regulator_bulk_data supplies[CX20709_NUM_SUPPLIES];
	int num_dai;
};

#define get_cx20709_priv(_codec_) ((struct cx20709_priv *) \
			snd_soc_codec_get_drvdata(codec))

static const struct reg_default cx20709_reg_defaults[] = {
	{ CX20709_CLOCK_DIVIDER,	0x00 },
	{ CX20709_PORT1_CONTROL,	0x00 },
	{ CX20709_PORT1_TX_FRAME,	0x00 },
	{ CX20709_PORT1_RX_FRAME,	0x00 },
	{ CX20709_PORT1_TX_SYNC,	0x00 },
	{ CX20709_PORT1_RX_SYNC,	0x00 },
	{ CX20709_PORT1_CONTROL2,	0x00 },
	{ CX20709_PORT1_RX_SLOT1,	0x00 },
	{ CX20709_PORT1_RX_SLOT2,	0x00 },
	{ CX20709_PORT1_TX_SLOT1,	0x00 },
	{ CX20709_PORT1_TX_SLOT2,	0x00 },
	{ CX20709_PORT1_TX_SLOT3,	0x00 },
	{ CX20709_PORT1_DELAY,		0x00 },
	{ CX20709_PORT2_CONTROL,	0x00 },
	{ CX20709_PORT2_FRAME,		0x00 },
	{ CX20709_PORT2_SYNC,		0x00 },
	{ CX20709_PORT2_SAMPLE,		0x00 },
	{ CX20709_PORT2_RX_SLOT1,	0x00 },
	{ CX20709_PORT2_RX_SLOT2,	0x00 },
	{ CX20709_PORT2_TX_SLOT1,	0x00 },
	{ CX20709_PORT2_TX_SLOT2,	0x00 },
	{ CX20709_ABCODE,	        0x01 },
	{ CX20709_FV_LO,	        0x00 },
	{ CX20709_FV_HI,	        0x00 },
	{ CX20709_VV_LO,	        0x00 },
	{ CX20709_VV_HI,		0x00 },
	{ CX20709_CHIP,			0X00 },
	{ CX20709_RELEASE,		0X00 },
	{ CX20709_GPIO_CTRL,		0xe0 },
	{ CX20709_GPIO_DIR,		0x1f },
	{ CX20709_BUTTON_STATUS,	0x00 },
	{ CX20709_DAC1_GAIN,		0x00 },
	{ CX20709_DAC2_GAIN,		0x00 },
	{ CX20709_CLASSD_GAIN,		0x08 },
	{ CX20709_DAC3_GAIN,		0x00 },
	{ CX20709_ADC1L_GAIN,		0x00 },
	{ CX20709_ADC1R_GAIN,		0x00 },
	{ CX20709_ADC2L_GAIN,		0x00 },
	{ CX20709_ADC2R_GAIN,		0x00 },
	{ CX20709_VOLUME_MUTE,		0x00 },
	{ CX20709_OUTPUT_CONTROL,	0x80 },
	{ CX20709_INPUT_CONTROL,	0x00 },
	{ CX20709_LINE1_GAIN,		0x00 },
	{ CX20709_LINE2_GAIN,		0x00 },
	{ CX20709_LINE3_GAIN,		0x00 },
	{ CX20709_STREAM3_RATE,		0x00 },
	{ CX20709_STREAM3_ROUTE,	0x00 },
	{ CX20709_STREAM4_RATE,		0x00 },
	{ CX20709_STREAM4_ROUTE,	0x00 },
	{ CX20709_STREAM5_RATE,		0x00 },
	{ CX20709_STREAM6_RATE,		0x00 },
	{ CX20709_DSP_ENDABLE,		0x00 },
	{ CX20709_DSP_ENDABLE2,		0x00 },
	{ CX20709_DSP_INIT,		0x00 },
	{ CX20709_DSP_INIT_NEWC,	0x00 },
	{ CX20709_LOWER_POWER,		0x00 },
	{ CX20709_DACIN_SOURCE,		0x00 },
	{ CX20709_DACSUBIN_SOURCE,	0x00 },
	{ CX20709_I2S1OUTIN_SOURCE,	0x00 },
	{ CX20709_USBOUT_SOURCE,	0x06 },
	{ CX20709_MIX0IN0_SOURCE,	0x01 },
	{ CX20709_MIX0IN1_SOURCE,	0x03 },
	{ CX20709_MIX0IN2_SOURCE,	0x04 },
	{ CX20709_MIX0IN3_SOURCE,	0x00 },
	{ CX20709_MIX1IN0_SOURCE,	0x00 },
	{ CX20709_MIX1IN1_SOURCE,	0x00 },
	{ CX20709_MIX1IN2_SOURCE,	0x00 },
	{ CX20709_MIX1IN3_SOURCE,	0x00 },
	{ CX20709_VOICEIN0_SOURCE,	0x02 },
	{ CX20709_I2S1OUTIN_SOURCE,	0x00 },
	{ CX20709_I2S2OUTIN_SOURCE,	0x00 },
	{ CX20709_DMIC_CONTROL,		0x43 },
	{ CX20709_I2S_OPTION,		0x00 }
};

/*
 * ADC/DAC Volume
 *
 * max : 0x00 : 5 dB
 *       ( 1 dB step )
 * min : 0xB6 : -74 dB
 */
static const DECLARE_TLV_DB_SCALE(main_tlv, -7400 , 100, 0);


/*
 * Capture Volume
 *
 * max : 0x00 : 12 dB
 *       ( 1.5 dB per step )
 * min : 0x1f : -35 dB
 */
static const DECLARE_TLV_DB_SCALE(line_tlv, -3500 , 150, 0);

static const char * const classd_gain_texts[] = {
"2.8W", "2.6W", "2.5W", "2.4W", "2.3W", "2.2W", "2.1W", "2.0W",
"1.3W", "1.25W", "1.2W", "1.15W", "1.1W", "1.05W", "1.0W", "0.9W"};

static const struct soc_enum classd_gain_enum =
	7SOC_ENUM_SINGLE(CX20709_CLASSD_GAIN, 0, 16, classd_gain_texts);

static const int apply_dsp_change(struct snd_soc_codec *codec)
{
	u16 try_loop = 50;
	mutex_lock(&codec->mutex);
	snd_soc_write(codec, CX20709_DSP_INIT_NEWC, snd_soc_read(
		codec, CX20709_DSP_INIT_NEWC) | 1);
	for (; try_loop; try_loop--) {
		if (0 == (snd_soc_read(codec, CX20709_DSP_INIT_NEWC) & 1)) {
			mutex_unlock(&codec->mutex);
			return 0;
		}
		udelay(1);
	}
	mutex_unlock(&codec->mutex);
	dev_err(codec->dev, "newc timeout\n");
	return -1;
}


static int dsp_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int mask = 1 << mc->shift;
	unsigned int val;

	snd_soc_update_bits(codec, reg, mask,
			ucontrol->value.integer.value[0] ? mask : 0);

	return apply_dsp_change(codec);
}

static const struct snd_kcontrol_new cx20709_controls[] = {
	SOC_SINGLE_TLV("Line 1 Vol", CX20709_LINE1_GAIN, 0, 32, 0, line_tlv),
	SOC_SINGLE_TLV("Line 2 Vol", CX20709_LINE2_GAIN, 0, 32, 0, line_tlv),
	SOC_SINGLE_TLV("Line 3 Vol", CX20709_LINE3_GAIN, 0, 32, 0, line_tlv),
	SOC_DOUBLE_S8_TLV("Left Speaker Volume", CX20709_DAC1_GAIN, -74, 5 ,
				main_tlv),
	SOC_DOUBLE_S8_TLV("Right Speaker Volume", CX20709_DAC2_GAIN, -74, 5 ,
				main_tlv),
	SOC_DOUBLE_S8_TLV("Mono Out Volume", CX20709_DAC3_GAIN, -74, 5,
		       main_tlv),
	SOC_DOUBLE_S8_TLV("Left ADC1 Gain", CX20709_ADC1L_GAIN, -74, 5,
		       main_tlv),
	SOC_DOUBLE_S8_TLV("Right ADC1 Gain", CX20709_ADC1R_GAIN, -74, 5,
				main_tlv),
	SOC_DOUBLE_S8_TLV("Left ADC2 Gain", CX20709_ADC1L_GAIN, -74, 5,
			main_tlv),
	SOC_DOUBLE_S8_TLV("Right ADC2 Gain", CX20709_ADC1R_GAIN, -74, 5,
				main_tlv),
	SOC_ENUM("Class-D Gain", classd_gain_enum),
	SOC_SINGLE_EXT("Sidetone Switch", CX20709_DSP_ENDABLE, 7, 7, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("Inbound NR Switch", CX20709_DSP_ENDABLE, 7, 5, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("Mic AGC Switch", CX20709_DSP_ENDABLE, 7, 4, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("Beam forming Switch", CX20709_DSP_ENDABLE, 7, 3, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("NR Switch", CX20709_DSP_ENDABLE, 7, 2, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("LEC Switch", CX20709_DSP_ENDABLE, 7, 1, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("AEC Switch", CX20709_DSP_ENDABLE, 7, 1, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("Tone Generator Switch", CX20709_DSP_ENDABLE2, 7, 5, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("DRC Switch", CX20709_DSP_ENDABLE2, 7, 2, 0,
					snd_soc_get_volsw, dsp_put),
	SOC_SINGLE_EXT("EQ Switch", CX20709_DSP_ENDABLE2, 7, 0, 0,
					snd_soc_get_volsw, dsp_put),
};

static const char * const stream3_mux_txt[] = {
	"Digital 1", "Digital 2", "No input", "USB TX2", "SPDIF"
};

static const struct soc_enum stream3_mux_enum =
	SOC_ENUM_SINGLE(CX20709_STREAM3_ROUTE, 3, 5, stream3_mux_txt);

static const struct snd_kcontrol_new stream3_mux =
	SOC_DAPM_ENUM("Stream 3 Mux", stream3_mux_enum);

static const char * const stream4_mux_txt[] = {
	"Digital 1", "Digital 2", "USB"
};
static const struct soc_enum stream4_mux_enum =
	SOC_ENUM_SINGLE(CX20709_STREAM4_ROUTE, 3, 3, stream4_mux_txt);

static const struct snd_kcontrol_new stream4_mux =
	SOC_DAPM_ENUM("Stream 4 Mux", stream4_mux_enum);


static const char * const dsp_input_mux_txt[] = {
	"None", "Stream 1", "Stream 2", "Stream 3", "Stream 4", "Scale Out",
	"Voice Out0", "Voice Out1", "Function Gen", "Mixer1 Out"
};


#define CX20709_DSP_INPUT_ENUM(_wname, _reg , _mux_enum)  \
	static const struct soc_enum _reg##dsp_input_mux_enum =  \
		SOC_ENUM_SINGLE(_reg, 0, 10, dsp_input_mux_txt); \
								 \
	static const struct snd_kcontrol_new _mux_enum =	 \
		SOC_DAPM_ENUM(_wname, _reg##dsp_input_mux_enum); \

CX20709_DSP_INPUT_ENUM("Mix0Input 0 Mux", CX20709_MIX0IN0_SOURCE,
		mix0in0_input_mux)
CX20709_DSP_INPUT_ENUM("Mix0Input 1 Mux", CX20709_MIX0IN1_SOURCE,
		mix0in1_input_mux)
CX20709_DSP_INPUT_ENUM("Mix0Input 2 Mux", CX20709_MIX0IN2_SOURCE,
		mix0in2_input_mux)
CX20709_DSP_INPUT_ENUM("Mix0Input 3 Mux", CX20709_MIX0IN3_SOURCE,
	       `	mix0in3_input_mux)
CX20709_DSP_INPUT_ENUM("Mix1Input 0 Mux", CX20709_MIX1IN0_SOURCE,
			mix1in0_input_mux)
CX20709_DSP_INPUT_ENUM("Mix1Input 1 Mux", CX20709_MIX1IN1_SOURCE,
			mix1in1_input_mux)
CX20709_DSP_INPUT_ENUM("Mix1Input 2 Mux", CX20709_MIX1IN2_SOURCE,
			mix1in2_input_mux)
CX20709_DSP_INPUT_ENUM("Mix1Input 3 Mux", CX20709_MIX1IN3_SOURCE,
			mix1in3_input_mux)
CX20709_DSP_INPUT_ENUM("VoiceIn Mux", CX20709_VOICEIN0_SOURCE,
	       voiice_input_mux)
CX20709_DSP_INPUT_ENUM("Stream 5 Mux", CX20709_I2S1OUTIN_SOURCE,
			stream5_input_mux)
CX20709_DSP_INPUT_ENUM("Stream 6 Mux", CX20709_I2S2OUTIN_SOURCE,
			stream6_input_mux)
CX20709_DSP_INPUT_ENUM("Stream 7 Mux", CX20709_DACIN_SOURCE,
	       stream7_input_mux)
CX20709_DSP_INPUT_ENUM("Stream 8 Mux", CX20709_DACSUBIN_SOURCE,
		stream8_input_mux)
CX20709_DSP_INPUT_ENUM("Stream 7 Mux", CX20709_USBOUT_SOURCE,
		stream9_input_mux)

static const struct snd_kcontrol_new hp_switch =
	SOC_DAPM_SINGLE("Switch", CX20709_OUTPUT_CONTROL, 0, 1, 0);

static const struct snd_kcontrol_new classd_switch =
	SOC_DAPM_SINGLE("Switch", CX20709_OUTPUT_CONTROL, 2, 1, 0);

static const struct snd_kcontrol_new lineout_switch =
	SOC_DAPM_SINGLE("Switch", CX20709_OUTPUT_CONTROL, 1, 1, 0);

static const struct snd_kcontrol_new function_gen_switch =
	SOC_DAPM_SINGLE("Switch", CX20709_DSP_ENDABLE, 5, 1, 0);

static const struct snd_kcontrol_new strm1_sel_mix[] = {
	SOC_DAPM_SINGLE("Digital Mic Switch", CX20709_DMIC_CONTROL, 6, 1, 1),
	SOC_DAPM_SINGLE("Line 1 Switch", CX20709_INPUT_CONTROL, 0, 1, 0),
	SOC_DAPM_SINGLE("Line 2 Switch", CX20709_INPUT_CONTROL, 1, 1, 0),
	SOC_DAPM_SINGLE("Line 3 Switch", CX20709_INPUT_CONTROL, 2, 1, 0),
};

static const struct snd_kcontrol_new strm2_sel_mix[] = {
	SOC_DAPM_SINGLE("Digital Mic Switch", CX20709_DMIC_CONTROL, 6, 1, 0),
};

static int newc_ev(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int nevent)
{
	/* execute the DSP change */
	struct snd_soc_codec *codec = w->codec;
	apply_dsp_change(codec);
	return 0;
}

static const struct snd_soc_dapm_widget cx20709_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DP1IN", PLAYBACK_STREAM_NAME_1, 0, SND_SOC_NOPM,
		       0, 0),
	SND_SOC_DAPM_AIF_IN("DP2IN", PLAYBACK_STREAM_NAME_2, 0, SND_SOC_NOPM,
		       0, 0),
	SND_SOC_DAPM_AIF_OUT("SPDIFOUT", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("USBOUT", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUTPUT("HPOUT"),
	SND_SOC_DAPM_OUTPUT("SPKOUT"),
	SND_SOC_DAPM_OUTPUT("MONOOUT"),
	SND_SOC_DAPM_OUTPUT("LINEOUT"),
	SND_SOC_DAPM_AIF_OUT("DP1OUT", CAPTURE_STREAM_NAME_1, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("DP2OUT", CAPTURE_STREAM_NAME_2, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_INPUT("LINEIN1"),
	SND_SOC_DAPM_INPUT("LINEIN2"),
	SND_SOC_DAPM_INPUT("LINEIN3"),
	SND_SOC_DAPM_INPUT("DMICIN"),
	SND_SOC_DAPM_INPUT("MICIN"),
	SND_SOC_DAPM_AIF_IN("USBTX2IN", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SPDIFIN", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("USBIN", NULL, 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_PGA("Digital Mic Enable", CX20709_DMIC_CONTROL, 7, 0,
			NULL, 0),
	SND_SOC_DAPM_MIXER("Stream 1 Mixer", CX20709_DSP_INIT_NEWC, 1, 0,
			strm1_sel_mix, ARRAY_SIZE(strm1_sel_mix)),
	SND_SOC_DAPM_PGA("Microphone", SND_SOC_NOPM, 0, 0, 0, 0),
	SND_SOC_DAPM_MIXER("Stream 2 Mixer", CX20709_DSP_INIT_NEWC, 2, 0,
			strm2_sel_mix, ARRAY_SIZE(strm2_sel_mix)),

	SND_SOC_DAPM_MUX("Stream 3 Mux", CX20709_DSP_INIT_NEWC, 3, 0,
			&stream3_mux),
	SND_SOC_DAPM_MUX("Stream 4 Mux", CX20709_DSP_INIT_NEWC, 4, 0,
			&stream4_mux),

	SND_SOC_DAPM_SIGGEN("TONE"),
	SND_SOC_DAPM_SWITCH("Function Generator", SND_SOC_NOPM, 0, 0,
			&function_gen_switch),

	/* playback dsp*/
	SND_SOC_DAPM_MUX("Mix0Input 0 Mux", SND_SOC_NOPM, 0, 0,
			&mix0in0_input_mux),
	SND_SOC_DAPM_MUX("Mix0Input 1 Mux", SND_SOC_NOPM, 0, 0,
			&mix0in1_input_mux),
	SND_SOC_DAPM_MUX("Mix0Input 2 Mux", SND_SOC_NOPM, 0, 0,
			&mix0in2_input_mux),
	SND_SOC_DAPM_MUX("Mix0Input 3 Mux", SND_SOC_NOPM, 0, 0,
			&mix0in3_input_mux),
	SND_SOC_DAPM_MIXER("Mixer 0 Mixer", SND_SOC_NOPM, 0, 0, 0, 0),
	SND_SOC_DAPM_PGA("Playback DSP", SND_SOC_NOPM, 0, 0, 0, 0),
	SND_SOC_DAPM_PGA("Scale Out", SND_SOC_NOPM, 0, 0, 0, 0),
	SND_SOC_DAPM_MUX("Mix1Input 0 Mux", SND_SOC_NOPM, 0, 0,
			&mix1in0_input_mux),
	SND_SOC_DAPM_MUX("Mix1Input 1 Mux", SND_SOC_NOPM, 0, 0,
			&mix1in1_input_mux),
	SND_SOC_DAPM_MUX("Mix1Input 2 Mux", SND_SOC_NOPM, 0, 0,
			&mix1in2_input_mux),
	SND_SOC_DAPM_MUX("Mix1Input 3 Mux", SND_SOC_NOPM, 0, 0,
			&mix1in3_input_mux),
	SND_SOC_DAPM_MIXER("Mixer 1 Mixer", SND_SOC_NOPM, 0, 0, 0, 0),

	/* voice dsp*/
	SND_SOC_DAPM_MUX("VoiceIn Mux", SND_SOC_NOPM, 0, 0, &voiice_input_mux),
	SND_SOC_DAPM_PGA("Voice DSP", SND_SOC_NOPM, 0, 0, 0, 0),
	SND_SOC_DAPM_PGA("Voice Out", SND_SOC_NOPM, 0, 0, 0, 0),

	/* stream 5*/
	SND_SOC_DAPM_MUX("Stream 5 Mux", CX20709_DSP_INIT_NEWC, 5, 0,
			&stream5_input_mux),

	/* stream 6*/
	SND_SOC_DAPM_MUX("Stream 6 Mux", CX20709_DSP_INIT_NEWC, 6, 0,
			&stream6_input_mux),

	/* Stream 7 */
	SND_SOC_DAPM_MUX("Stream 7 Mux", CX20709_DSP_INIT_NEWC, 7, 0,
			&stream7_input_mux),
	/*FIX ME, there is a register to switch output path.*/
	SND_SOC_DAPM_PGA("SPDIF Out", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Headphone Switch", CX20709_OUTPUT_CONTROL, 0, 0,
			NULL, 0),
	SND_SOC_DAPM_PGA("Class D Switch", CX20709_OUTPUT_CONTROL, 2, 0,
			NULL, 0),
	SND_SOC_DAPM_PGA("Line Out Switch", CX20709_OUTPUT_CONTROL, 1, 0,
			NULL, 0),

	/* Stream 8 */
	SND_SOC_DAPM_MUX("Stream 8 Mux", CX20709_DSP_INIT, 0, 0,
			&stream8_input_mux),
	SND_SOC_DAPM_PGA("Mono Out", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Stream 9 */
	SND_SOC_DAPM_MUX("Stream 9 Mux", CX20709_DSP_INIT_NEWC, 1, 0,
			&stream9_input_mux),
	SND_SOC_DAPM_PGA("USB Out", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_POST("NEWC", newc_ev),
};

#define CX20709_DSP_MUX_ROUTES(widget)  \
	{widget, "Stream 1", "Stream 1 Mixer"}, \
	{widget, "Stream 2", "Stream 2 Mixer"}, \
	{widget, "Stream 3", "Stream 3 Mux"}, \
	{widget, "Stream 4", "Stream 4 Mux"}, \
	{widget, "Function Gen","Function Generator"}

#define CX20709_OUTPUT_SOURCE_MUX_ROUTES(_wname) \
	{_wname, "Stream 1", "Stream 1 Mixer"}, \
	{_wname, "Stream 2", "Stream 2 Mixer"}, \
	{_wname, "Stream 3", "Stream 3 Mux"}, \
	{_wname, "Stream 4", "Stream 4 Mux"}, \
	{_wname, "Scale Out", "Scale Out"}, \
	{_wname, "Voice Out0", "Voice Out"}, \
	{_wname, "Function Gen", "Function Generator"}, \
	{_wname, "Mixer1 Out", "Mixer 1 Mixer"} \

static const struct snd_soc_dapm_route cx20709_routes[] = {
	/* stream 1 */
	{"Digital Mic Enable", NULL, "DMICIN"},
	{"Stream 1 Mixer", "Line 1 Switch", "LINEIN1"},
	{"Stream 1 Mixer", "Line 2 Switch", "LINEIN2"},
	{"Stream 1 Mixer", "Line 3 Switch", "LINEIN3"},
	{"Stream 1 Mixer", "Digital Mic Switch", "Digital Mic Enable"},

	/* stream 2 */
	{"Microphone", NULL, "MICIN"},
	{"Stream 2 Mixer", NULL, "Microphone"},
	{"Stream 2 Mixer", "Digital Mic Switch", "Digital Mic Enable"},

	/* stream 3*/
	{"Stream 3 Mux", "Digital 1", "DP1IN"},
	{"Stream 3 Mux", "Digital 2", "DP2IN"},
	{"Stream 3 Mux", "USB TX2", "USBTX2IN"},
	{"Stream 3 Mux", "SPDIF", "SPDIFIN"},

	/* straem 4*/
	{"Stream 4 Mux", "Digital 1", "DP1IN"},
	{"Stream 4 Mux", "Digital 2", "DP2IN"},
	{"Stream 4 Mux", "USB", "USBIN"},

	/* Function Generator */
	{"Function Generator", "Switch", "TONE" },

	/*Mixer 0 + Playback DSP*/
	CX20709_DSP_MUX_ROUTES("Mix0Input 0 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix0Input 1 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix0Input 2 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix0Input 3 Mux"),
	{"Mixer 0 Mixer", NULL, "Mix0Input 0 Mux"},
	{"Mixer 0 Mixer", NULL, "Mix0Input 1 Mux"},
	{"Mixer 0 Mixer", NULL, "Mix0Input 2 Mux"},
	{"Mixer 0 Mixer", NULL, "Mix0Input 3 Mux"},
	{"Playback DSP", NULL, "Mixer 0 Mixer"},
	{"Scale Out", NULL, "Playback DSP"},

	/*Mixer 1*/
	CX20709_DSP_MUX_ROUTES("Mix1Input 0 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix1Input 1 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix1Input 2 Mux"),
	CX20709_DSP_MUX_ROUTES("Mix1Input 3 Mux"),
	{"Mixer 1 Mixer", NULL, "Mix1Input 0 Mux"},
	{"Mixer 1 Mixer", NULL, "Mix1Input 1 Mux"},
	{"Mixer 1 Mixer", NULL, "Mix1Input 2 Mux"},
	{"Mixer 1 Mixer", NULL, "Mix1Input 3 Mux"},

	/* Voice Processing */
	CX20709_DSP_MUX_ROUTES("VoiceIn Mux"),
	{"Voice DSP", NULL, "VoiceIn Mux"},
	{"Voice Out", NULL, "Voice DSP"},

	/* Stream 5 */
	CX20709_OUTPUT_SOURCE_MUX_ROUTES("Stream 5 Mux"),
	{"DP1OUT", NULL, "Stream 5 Mux"},

	/* Stream 6 */
	CX20709_OUTPUT_SOURCE_MUX_ROUTES("Stream 6 Mux"),
	{"DP2OUT", NULL, "Stream 6 Mux"},

	/* Stream 7 */
	CX20709_OUTPUT_SOURCE_MUX_ROUTES("Stream 7 Mux"),
	{"SPDIF Out", NULL, "Stream 7 Mux"},
	{"Headphone Switch", "Switch", "Stream 7 Mux"},
	{"Class D Switch", "Switch", "Stream 7 Mux"},
	{"Line Out Switch", "Switch", "Stream 7 Mux"},

	/* Stream 8 */
	CX20709_OUTPUT_SOURCE_MUX_ROUTES("Stream 8 Mux"),
	{"Mono Out", NULL, "Stream 8 Mux"},

	/* Stream 9 */
	CX20709_OUTPUT_SOURCE_MUX_ROUTES("Stream 9 Mux"),
	{"USB Out", NULL, "Stream 9 Mux"},

	/* DAPM Endpoint */
	{"HPOUT", NULL, "Headphone Switch"},
	{"SPKOUT", NULL, "Class D Switch"},
	{"LINEOUT", NULL, "Line Out Switch"},
	{"MONOOUT", NULL, "Class D Switch"},
	{"LINEOUT", NULL, "Mono Out"},
	{"USBOUT", NULL, "USB Out"},
};

static int cx20709_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cx20709_priv *cx20709 = get_cx20709_priv(codec);
	u8 val = 0;
	u8 sample_size;
	u32 bit_rate;
	u32 frame_size;
	u32 num_ch = 2;

	/*turn off bit clock output*/
	snd_soc_update_bits(codec, CX20709_CLOCK_DIVIDER,
			dai->id ? 0x0f << 4 : 0x0f,
			dai->id ? 0x0f <<4: 0xf);

	switch (params_format(params)) {
	case SNDRV_PCM_FMTBIT_A_LAW:
		val |= 0 <<4;
		sample_size = 1;
		break;
	case SNDRV_PCM_FMTBIT_MU_LAW:
		val |=  1 << 4;
		sample_size = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		val |=  2 << 4;
		sample_size = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val |=  3 << 4;
		sample_size = 3;
		break;
	default:
		dev_warn(dai->dev, "Unsupported format %d\n",
				params_format(params));
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case  8000:	val |= 0;	break;
	case 11025:	val |= 1;	break;
	case 16000:	val |= 2;	break;
	case 22050:	val |= 3;	break;
	case 24000:	val |= 4;	break;
	case 32000:	val |= 5;	break;
	case 44100:	val |= 6;	break;
	case 48000:	val |= 7;	break;
	case 88200:	val |= 8;	break;
	case 96000:	val |= 9;	break;
	default:
		dev_warn(dai->dev, "Unsupported sample rate %d\n",
				params_rate(params));
		return -EINVAL;
	}

	/*update input rate*/
	snd_soc_update_bits(codec, dai->id ?
			CX20709_STREAM4_RATE:CX20709_STREAM3_RATE, 0x3f, val);

	/*update output rate*/
	snd_soc_update_bits(codec, dai->id ?
			CX20709_STREAM6_RATE:CX20709_STREAM5_RATE, 0x3f, val);

	/*set bit clock*/
	frame_size = (sample_size * 8) * num_ch;
	bit_rate = frame_size * params_rate(params);

	dev_info(dai->dev, "bit rate at %uHz, master = %d\n",
			bit_rate, cx20709->master[dai->id]);

	dev_info(dai->dev, "sample size = %d bytes, sample rate = %uHz\n",
			sample_size, params_rate(params));

	if (dai->id == 0) {
		snd_soc_write(codec, CX20709_PORT1_TX_FRAME, frame_size/8-1);
		snd_soc_write(codec, CX20709_PORT1_RX_FRAME, frame_size/8-1);
		/*TODO: only I2S mode is implemented.*/
		snd_soc_write(codec, CX20709_PORT1_TX_SYNC,
				frame_size/num_ch-1);
		snd_soc_write(codec, CX20709_PORT1_RX_SYNC,
				frame_size/num_ch-1);
		val = sample_size-1;
		val |= val << 2;
		/*TODO : implement PassThru mode*/
		snd_soc_write(codec, CX20709_PORT1_CONTROL2, val);

	} else {
		snd_soc_write(codec, CX20709_PORT2_FRAME, frame_size/8-1);
		/*TODO: only I2S mode is implemented.*/
		snd_soc_write(codec, CX20709_PORT2_SYNC, frame_size/num_ch-1);
		val = sample_size-1;
		/*TODO: implement PassThru mode*/
		/*snd_soc_update_bits(codec, CX20709_PORT2_SAMPLE,0x02,val);*/
		snd_soc_write(codec, CX20709_PORT2_SAMPLE,val);
	}

	bit_rate/=1000;
	bit_rate*=1000;

	if (!cx20709->master[dai->id])
		val = 0xf;
	else {
		switch (bit_rate)
		{
		case 6144000:	val= 1;	break;
		case 3072000:	val= 2;	break;
		case 2048000:	val= 3;	break;
		case 1536000:	val= 4;	break;
		case 1024000:	val= 5;	break;
		case 568000:	val= 6;	break;
		case 512000:	val= 7;	break;
		case 384000:	val= 7;	break;
		case 256000:	val= 9;	break;
		case 5644000:	val= 10;	break;
		case 2822000:	val= 11;	break;
		case 1411000:	val= 12;	break;
		case 705000:	val= 13;	break;
		case 352000:	val= 13;	break;
		default:
			dev_warn(dai->dev, "Unsupported bit rate %uHz\n",
				bit_rate);
			return -EINVAL;
		}
	}

	snd_soc_update_bits(codec,CX20709_CLOCK_DIVIDER,
		dai->id ? 0x0f << 4 : 0x0f,
		dai->id ? val << 4 : val);

	return 0;
}

static int cx20709_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	return snd_soc_update_bits(codec, CX20709_VOLUME_MUTE,
		0x03, mute ? 0x03 : 0);
}

static int cx20709_set_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cx20709_priv  *cx20709 = get_cx20709_priv(codec);

	dev_dbg(dai->dev, "using MCLK at %uHz\n",
			freq);
	u8 val = snd_soc_read(codec, CX20709_I2S_OPTION);
	val &= ~0x10;
	if(dir == SND_SOC_CLOCK_OUT) {
		switch(freq) {
		case 2048000:	val |= 0; break;
		case 4096000:	val |= 1; break;
		case 5644000:	val |= 2; break;
		case 6144000:	val |= 3; break;
		case 8192000:	val |= 4; break;
		case 11289000:	val |= 5; break;
		case 12288000:	val |= 6; break;
		case 24576000:	val |= 10; break;
		case 22579000:	val |= 11; break;
		default:
			dev_err(dai->dev, "Unsupport MCLK rate %uHz!\n", freq);
			return -EINVAL;
		}
		val |= 0x10; /*enable MCLK output*/
		snd_soc_write(codec,CX20709_I2S_OPTION, val);
	} else
		snd_soc_write(codec,CX20709_I2S_OPTION, val);

	cx20709->sysclk = freq;
	return 0;
}

static int cx20709_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cx20709_priv *cx20709 = get_cx20709_priv(codec);
	uint8_t is_pcm = 0;
	uint8_t is_frame_invert = 0;
	uint8_t is_clk_invert = 0;
	uint8_t is_right_j = 0;
	uint8_t is_one_delay = 0;
	uint8_t  val;

	if (dai->id > NUM_OF_DAI) {
		dev_err(dai->dev, "Unknown dai configuration,dai->id = %d\n",
				dai->id);
		return - EINVAL;
	}

	/* set master/slave audio interface*/
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		cx20709->master[dai->id] = 0;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		cx20709->master[dai->id] = 1;
		break;
	default:
		dev_err(dai->dev, "Unsupported master/slave configuration\n");
		return - EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_B:
	/*PCM short frame sync */
		is_pcm =1;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	/*PCM short frame sync with one cycle delay */
		is_pcm =1;
		is_one_delay =1;
		break;
	case SND_SOC_DAIFMT_I2S:
	/*I2S*/
		is_one_delay = 1;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
	/*I2S right justified*/
		is_right_j = 1;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
	/*I2S without delay*/
		break;
	default:
		dev_err(dai->dev, "Unsupported dai format %d\n",
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		if (is_pcm) {
			dev_err(dai->dev,
				"Can't support invert frame in PCM mode\n");
			return -EINVAL;
		}
		is_frame_invert  =1;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		is_clk_invert  =1;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		if (is_pcm) {
			dev_err(dai->dev,
				"Can't support invert frame in PCM mode\n");
			return -EINVAL;
		}
		is_frame_invert = 1;
		is_clk_invert = 1;
		break;
	}

	val = (is_one_delay << 7) |
		(is_right_j << 6) |
		(is_clk_invert << 3) |
		(is_clk_invert << 2) |
		(is_frame_invert << 1) |
		(is_pcm) ;
	snd_soc_update_bits(codec,dai->id ? CX20709_PORT2_CONTROL :
		CX20709_PORT1_CONTROL, 0xc0, val);
	return 0;
}

static const struct snd_soc_dai_ops cx20709_dai_ops = {
	.hw_params = cx20709_hw_params,
	.set_fmt = cx20709_set_fmt,
	.set_sysclk = cx20709_set_sysclk,
};

#define CX20709_RATES	( \
	SNDRV_PCM_RATE_8000  \
	| SNDRV_PCM_RATE_11025 \
	| SNDRV_PCM_RATE_16000 \
	| SNDRV_PCM_RATE_22050 \
	| SNDRV_PCM_RATE_32000 \
	| SNDRV_PCM_RATE_44100 \
	| SNDRV_PCM_RATE_48000 \
	| SNDRV_PCM_RATE_88200 \
	| SNDRV_PCM_RATE_96000)

#define CX20709_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
	| SNDRV_PCM_FMTBIT_S16_BE \
	| SNDRV_PCM_FMTBIT_MU_LAW \
	| SNDRV_PCM_FMTBIT_A_LAW)

static struct snd_soc_dai_driver cx20709_dai[NUM_OF_DAI] = {
	{
		.name = DAI_DP1_NAME,
		.playback = {
			.stream_name = PLAYBACK_STREAM_NAME_1,
			.channels_min = 1,
			.channels_max = 2,
			.rates = CX20709_RATES,
			.formats = CX20709_FORMATS,
		},
		.capture = {
			.stream_name = CAPTURE_STEAM_NAME_1,
			.channels_min = 1,
			.channels_max = 2,
			.rates = CX20709_RATES,
			.formats = CX20709_FORMATS,
		},
		.ops = &cx20709_dai_ops,
		.symmetric_rates = 1
	},{
		.name = DAI_DP2_NAME,
		.playback = {
			.stream_name = PLAYBACK_STREAM_NAME_2,
			.channels_min = 1,
			.channels_max = 2,
			.rates = CX20709_RATES,
			.formats = CX20709_FORMATS,
		},
		.capture = {
			.stream_name = CAPTURE_STEAM_NAME_2,
			.channels_min = 1,
			.channels_max = 2,
			.rates = CX20709_RATES,
			.formats = CX20709_FORMATS,
		},
		.ops = &cx20709_dai_ops,
		.symmetric_rates = 1
	}
};

static int cx20709_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	int ret;
	struct cx20709_priv *cx20709 = get_cx20709_priv(codec);
	switch(level)
	{
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level = SND_SOC_BIAS_OFF) {
			regcache_cache_only(cx20709->regmap, false);
			regcache_sync(cx20709->regmap);
			/*wake up*/
			snd_soc_write(codec, CX20709_LOWER_POWER, 0x00);
			msleep(200);
		}
		break;
	case SND_SOC_BIAS_OFF:
		/*deep sleep mode*/
		snd_soc_write(codec, CX20709_DSP_INIT_NEWC, 1);
		apply_dsp_change(codec);
		snd_soc_write(codec, CX20709_LOWER_POWER, 0xe0);
		regcache_cache_only(cx20709->regmap, true);
		regcache_mark_dirty(cx20709->regmap);
		break;
	}

	codec->dapm.bias_level = level;
	return 0;
}


static const struct snd_soc_component_driver cx20709_soc_component_driver = {
	.controls		= cx20709_controls,
	.num_controls		= ARRAY_SIZE(cx20709_controls),
	.dapm_widgets		= cx20709_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(cx20709_dapm_widgets),
	.dapm_routes		= cx20709_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(cx20709_dapm_routes),
	.use_pmdown_time	= 1,
        .idle_bias_on		= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};


static bool cx20709_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case CX20709_ABCODE:
		case CX20709_FV_LO:
		case CX20709_FV_HI:
		case CX20709_VV_LO:
		case CX20709_VV_HI:
		case CX20709_CHIP:
		case CX20709_RELEASE:
		case CX20709_ABCODE:
		case CX20709_UPDATE_CTR:
		case CX20709_DSP_INIT_NEWC:
		return 1;
	default:
		return 0;
	}
}

const struct regmap_config cx20709_regmap_config = {
	.max_register = CX20709_MAX_REG,
	.volatile_reg = cx20709_volatile_vregister,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = cx20709_reg,
	.num_reg_defaults = ARRAY_SIZE(cx20709_reg),
};
EXPORT_SYMBOL_GPL(cx20709_regmap_config);

int cx20709_reset(struct device *dev)
{
	int ret = 0;

	// try hardware reset if avaiable.
	if (cx20709->reset) {
		gpiod_set_value_cansleep(cx20709->reset, 1);
		mdelay(5);
		gpiod_set_value_cansleep(cx20709->reset, 0);
	} else {
		ret = cx20709_soft_reset(cx20709);
		if (ret < 0) {
			dev_err(dev, "Failed to issue reset: %d\n", ret);
			return ret;
		}
	}
	//FIXME: need to figure out how many delay is actually required.
	mdelay(10);

	return ret;
}

int cx20709_probe(struct device *dev, struct regmap *regmap,
		enum cx20709_type type)
{
	struct cx20709_priv *cx20709;
	int ret;
	unsigned int chip_ver;

	cx20709 = devm_kzalloc(dev, sizeof(*cx20709), GFP_KERNEL);

	if (cx20709 == NULL)
		return -ENOMEM;

	dev_set_drvdata(dev, cx20709);

	cx20709->dev = dev;
	cx20709->regmap = regmap;
	cx20709->type = type;
	cx20709->reset = devm_gpiod_get_optional(dev, "reset",
			GPIOD_OUT_LOW);

	if (IS_ERR(cx20709->reset)) {
		ret = PTR_ERR(cx20709->reset);
		dev_err(dev, "Failed to get reset line: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(cx20709->supplies); i++)
		cx20709->supplies[i].supply = cx20709_supply_name[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(supplies),
			cx20709->supplies);

	if (ret) {
		dev_err(dev, "Failed to request supplies: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(cx20709->supplies),
			cx20709->supplies);
	if (ret) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	ret = cx20709_reset(dev);
	if (ret) {
		dev_err(dev, "Failed to reset device: %d\n", ret);
		goto err_regulator_enable;
	}

	ret = regmap_read(regmap, CX20709_CHIP_VERSION, &chip_ver);
	if (ret < 0) {
		dev_err(dev, "Failed to read chip version: %d\n", ret);
		goto err_regulator_enable;
	}

	dev_info(dev, "chip type: cx2070%c\n", chip_ver);

	ret = devm_snd_soc_register_component(dev, &soc_component_dev_cx20709,
		cx20709_dai, ARRAY_SIZE(cx20709_dai));

	if (ret < 0) {
		dev_err(dev, "Failed to registr CODEC: %d\n", ret);
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;
err_regulator_enable:
	regulator_bulk_disable(ARRAY_SIZE(cx20709->supplies),
		       cx20709->supplies);
	return ret;
}
EXPORT_SYMBOL_GPL(cx20709_probe);

void cx20709_remove(struct device *dev)
{
	pm_runtime_disable(dev);
}
EXPORT_SYMBOL_GPL(cx20709_remove);


	soc_codec_dev_cx20709.probe = cx20709_probe;
	soc_codec_dev_cx20709.remove = cx20709_remove;
	soc_codec_dev_cx20709.suspend = cx20709_suspend;
	soc_codec_dev_cx20709.resume = cx20709_resume;

	soc_codec_dev_cx20709.controls = cx20709_snd_controls;
	soc_codec_dev_cx20709.num_controls = ARRAY_SIZE(cx20709_snd_controls);
	soc_codec_dev_cx20709.dapm_widgets = cx20709_dapm_widgets;
	soc_codec_dev_cx20709.num_dapm_widgets =
			ARRAY_SIZE(cx20709_dapm_widgets);
	soc_codec_dev_cx20709.dapm_routes = cx20709_routes;
	soc_codec_dev_cx20709.num_dapm_routes = ARRAY_SIZE(cx20709_routes);
	soc_codec_dev_cx20709.set_bias_level = cx20709_set_bias_level;
#ifdef CONFIG_SND_SOC_CX20709_FW_PATCH
	dev_info(cx20709->dev, "waiting for firmware %s\n",
			CX20709_FIRMWARE_FILENAME);

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		CX20709_FIRMWARE_FILENAME, cx20709->dev, GFP_KERNEL,
		cx20709, cx20709_firmware_cont);
	if (ret) {
		dev_err(cx20709->dev, "Failed to load firmware %x\n",
				CX20709_FIRMWARE_FILENAME);
		return ret;
	}
	continue_probe = false;
#endif
	if (continue_probe) {
		ret = snd_soc_register_codec(cx20709->dev,
				&soc_codec_dev_cx20709, soc_codec_cx20709_dai,
				NUM_OF_DAI);
		if (ret < 0)
			dev_err(cx20709->dev,
				"Failed to register codec: %d\n", ret);
		else
			dev_dbg(cx20709->dev,
				"%s: Register codec.\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(cx20709_probe);

void cx20709_remove(struct device *dev)
{
	pm_runtime_disable(dev);
}
EXPORT_SYMBOL_GPL(cx20709_remove);

#if defined(CONFIG_PM)
static int cx20709_runtime_resume(struct device *dev)
{
	struct cx20709_priv *cx20709 = dev_get_drvdata(dev);
	int ret ;

	ret = regulator_bulk_enable(ARRAY_SIZE(cx20709->supplies),
			cx20709->supplies);
	if (ret) {
		dev_err(cx20709->dev,"Failed to enable supplies: %d\n", ret);
		return ret;
	}

	cx20709_reset(dev);
	regcache_sync(cx20709->regmap);

	return 0;
}
static int runtime_suspend(struct device *dev)
{
	struct cx20709_priv *cx20709 = dev_get_drvdata(dev);

	//FIXME: add power down procedure

	regulator_bulk_disable(ARRAY_SIZE(cx20709->supplies),
			cx20709->supplies);
}
#endif

const struct dev_pm_ops cx20709_pm = {
	SET_RUNTIME_PM_OPS(cx20709_runtime_suspend, cx20709_runtime_resume,
			NULL)
};
EXPORT_SYMBOL_GPL(cx20709_pm);
MODULE_DESCRIPTION("ASoC CX20709 Codec Driver");
MODULE_AUTHOR("Simon Ho <simon.ho@synaptics.com>");
MODULE_LICENSE("GPL v2");
