// license:BSD-3-Clause
// copyright-holders:Luca Elia, Hau
/***************************************************************************

Hyper Duel
(c)1993 Technosoft
TEC442-A(Japan)

TMP68000N-10 x2
YM2151,YM3012
OKI M6295
OSC:  4.0000MHz, 20.0000MHz, 26.6660MHz
Imagetek Inc 14220 071


Magical Error wo Sagase
(c)1994 Technosoft / Jaleco
TEC5000(Japan)

TMP68000N-10 x2
YM2413
OKI M6295
OSC:  3.579545MHz, 4.0000MHz, 20.0000MHz, 26.6660MHz
Imagetek Inc 14220 071

--
Written by Hau
03/29/2009
based on driver from drivers/metro.cpp by Luca Elia
spthx to kikur,Cha,teioh,kokkyu,teruchu,aya,sgo
---

Magical Error
different sized sound / shared region (or the mem map needs more alterations?)
fix comms so it boots, it's a bit of a hack for hyperduel at the moment ;-)

***************************************************************************/

#include "emu.h"

#include "cpu/m68000/m68000.h"
#include "machine/timer.h"
#include "sound/okim6295.h"
#include "sound/ym2151.h"
#include "sound/ym2413.h"
#include "video/imagetek_i4100.h"

#include "screen.h"
#include "speaker.h"


#define RASTER_LINES 262
#define FIRST_VISIBLE_LINE 0
#define LAST_VISIBLE_LINE 223

class hyprduel_state : public driver_device
{
public:
	hyprduel_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_irq_enable(*this, "irq_enable")
		, m_sharedram(*this, "sharedram%u", 1)
		, m_maincpu(*this, "maincpu")
		, m_subcpu(*this, "sub")
		, m_vdp(*this, "vdp")
	{ }

	DECLARE_READ16_MEMBER(irq_cause_r);
	DECLARE_WRITE16_MEMBER(irq_cause_w);
	DECLARE_WRITE16_MEMBER(subcpu_control_w);
	DECLARE_READ16_MEMBER(hyprduel_cpusync_trigger1_r);
	DECLARE_WRITE16_MEMBER(hyprduel_cpusync_trigger1_w);
	DECLARE_READ16_MEMBER(hyprduel_cpusync_trigger2_r);
	DECLARE_WRITE16_MEMBER(hyprduel_cpusync_trigger2_w);
	DECLARE_DRIVER_INIT(magerror);
	DECLARE_DRIVER_INIT(hyprduel);
	DECLARE_MACHINE_START(hyprduel);
	DECLARE_MACHINE_START(magerror);
	TIMER_CALLBACK_MEMBER(vblank_end_callback);
	DECLARE_WRITE_LINE_MEMBER(vdp_blit_end_w);
	TIMER_DEVICE_CALLBACK_MEMBER(interrupt);
	
	void magerror(machine_config &config);
	void hyprduel(machine_config &config);
	void i4220_config(machine_config &config);
	void hyprduel_map(address_map &map);
	void hyprduel_map2(address_map &map);
	void magerror_map(address_map &map);
	void magerror_map2(address_map &map);
protected:
	virtual void machine_reset() override;

private:
	/* memory pointers */
	required_shared_ptr<uint16_t> m_irq_enable;
	required_shared_ptr_array<uint16_t, 3> m_sharedram;

	/* misc */
	emu_timer *m_vblank_end_timer;
	int       m_blitter_bit;
	int       m_requested_int;
	int       m_subcpu_resetline;
	int       m_cpu_trigger;
	int       m_int_num;

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_subcpu;
	required_device<imagetek_i4220_device> m_vdp;

	void update_irq_state(  );
};


/***************************************************************************
                                Interrupts
***************************************************************************/

void hyprduel_state::update_irq_state(  )
{
	int irq = m_requested_int & ~*m_irq_enable;

	m_maincpu->set_input_line(3, (irq & m_int_num) ? ASSERT_LINE : CLEAR_LINE);
}

TIMER_CALLBACK_MEMBER(hyprduel_state::vblank_end_callback)
{
	m_requested_int &= ~param;
}

TIMER_DEVICE_CALLBACK_MEMBER(hyprduel_state::interrupt)
{
	int line = param;

	if (line == 0) /* TODO: fix this! */
	{
		m_requested_int |= 0x01;        /* vblank */
		m_requested_int |= 0x20;
		m_maincpu->set_input_line(2, HOLD_LINE);
		/* the duration is a guess */
		m_vblank_end_timer->adjust(attotime::from_usec(2500), 0x20);
	}
	else
		m_requested_int |= 0x12;        /* hsync */

	update_irq_state();
}

READ16_MEMBER(hyprduel_state::irq_cause_r)
{
	return m_requested_int;
}

WRITE16_MEMBER(hyprduel_state::irq_cause_w)
{
	if (ACCESSING_BITS_0_7)
	{
		if (data == m_int_num)
			m_requested_int &= ~(m_int_num & ~*m_irq_enable);
		else
			m_requested_int &= ~(data & *m_irq_enable);

		update_irq_state();
	}
}


WRITE16_MEMBER(hyprduel_state::subcpu_control_w)
{
	switch (data)
	{
		case 0x0d:
		case 0x0f:
		case 0x01:
			if (!m_subcpu_resetline)
			{
				m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
				m_subcpu_resetline = 1;
			}
			break;

		case 0x00:
			if (m_subcpu_resetline)
			{
				m_subcpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
				m_subcpu_resetline = 0;
			}
			m_maincpu->spin_until_interrupt();
			break;

		case 0x0c:
		case 0x80:
			m_subcpu->set_input_line(2, HOLD_LINE);
			break;
	}
}


READ16_MEMBER(hyprduel_state::hyprduel_cpusync_trigger1_r)
{
	if (m_cpu_trigger == 1001)
	{
		machine().scheduler().trigger(1001);
		m_cpu_trigger = 0;
	}

	return m_sharedram[0][0x000408 / 2 + offset];
}

WRITE16_MEMBER(hyprduel_state::hyprduel_cpusync_trigger1_w)
{
	COMBINE_DATA(&m_sharedram[0][0x00040e / 2 + offset]);

	if (((m_sharedram[0][0x00040e / 2] << 16) + m_sharedram[0][0x000410 / 2]) != 0x00)
	{
		if (!m_cpu_trigger && !m_subcpu_resetline)
		{
			m_maincpu->spin_until_trigger(1001);
			m_cpu_trigger = 1001;
		}
	}
}


READ16_MEMBER(hyprduel_state::hyprduel_cpusync_trigger2_r)
{
	if (m_cpu_trigger == 1002)
	{
		machine().scheduler().trigger(1002);
		m_cpu_trigger = 0;
	}

	return m_sharedram[2][(0xfff34c - 0xfe4000) / 2 + offset];
}

WRITE16_MEMBER(hyprduel_state::hyprduel_cpusync_trigger2_w)
{
	COMBINE_DATA(&m_sharedram[0][0x000408 / 2 + offset]);

	if (ACCESSING_BITS_8_15)
	{
		if (!m_cpu_trigger && !m_subcpu_resetline)
		{
			m_maincpu->spin_until_trigger(1002);
			m_cpu_trigger = 1002;
		}
	}
}

WRITE_LINE_MEMBER(hyprduel_state::vdp_blit_end_w)
{
	m_requested_int |= 1 << m_blitter_bit;
	update_irq_state();
}

/***************************************************************************
                                Memory Maps
***************************************************************************/

void hyprduel_state::hyprduel_map(address_map &map)
{
	map(0x000000, 0x07ffff).rom();
	map(0x400000, 0x47ffff).m(m_vdp, FUNC(imagetek_i4220_device::v2_map));
	map(0x4788a2, 0x4788a3).rw(this, FUNC(hyprduel_state::irq_cause_r), FUNC(hyprduel_state::irq_cause_w));   /* IRQ Cause,Acknowledge */
	map(0x4788a4, 0x4788a5).ram().share("irq_enable");      /* IRQ Enable */
	map(0x800000, 0x800001).w(this, FUNC(hyprduel_state::subcpu_control_w));
	map(0xc00000, 0xc07fff).ram().share("sharedram1");
	map(0xe00000, 0xe00001).portr("SERVICE").nopw();
	map(0xe00002, 0xe00003).portr("DSW");
	map(0xe00004, 0xe00005).portr("P1_P2");
	map(0xe00006, 0xe00007).portr("SYSTEM");
	map(0xfe0000, 0xfe3fff).ram().share("sharedram2");
	map(0xfe4000, 0xffffff).ram().share("sharedram3");
}

void hyprduel_state::hyprduel_map2(address_map &map)
{
	map(0x000000, 0x003fff).ram().share("sharedram1");                      /* shadow ($c00000 - $c03fff : vector) */
	map(0x004000, 0x007fff).readonly().nopw().share("sharedram3");         /* shadow ($fe4000 - $fe7fff : read only) */
	map(0x400000, 0x400003).rw("ymsnd", FUNC(ym2151_device::read), FUNC(ym2151_device::write)).umask16(0x00ff);
	map(0x400005, 0x400005).rw("oki", FUNC(okim6295_device::read), FUNC(okim6295_device::write));
	map(0x800000, 0x800001).noprw();
	map(0xc00000, 0xc07fff).ram().share("sharedram1");
	map(0xfe0000, 0xfe3fff).ram().share("sharedram2");
	map(0xfe4000, 0xffffff).ram().share("sharedram3");
}


/* Magical Error - video is at 8x now */

void hyprduel_state::magerror_map(address_map &map)
{
	map(0x000000, 0x07ffff).rom();
	map(0x400000, 0x400001).w(this, FUNC(hyprduel_state::subcpu_control_w));
	map(0x800000, 0x87ffff).m(m_vdp, FUNC(imagetek_i4220_device::v2_map));
	map(0x8788a2, 0x8788a3).rw(this, FUNC(hyprduel_state::irq_cause_r), FUNC(hyprduel_state::irq_cause_w));   /* IRQ Cause, Acknowledge */
	map(0x8788a4, 0x8788a5).ram().share("irq_enable");      /* IRQ Enable */
	map(0xc00000, 0xc1ffff).ram().share("sharedram1");
	map(0xe00000, 0xe00001).portr("SERVICE").nopw();
	map(0xe00002, 0xe00003).portr("DSW");
	map(0xe00004, 0xe00005).portr("P1_P2");
	map(0xe00006, 0xe00007).portr("SYSTEM");
	map(0xfe0000, 0xfe3fff).ram().share("sharedram2");
	map(0xfe4000, 0xffffff).ram().share("sharedram3");
}

void hyprduel_state::magerror_map2(address_map &map)
{
	map(0x000000, 0x003fff).ram().share("sharedram1");                      /* shadow ($c00000 - $c03fff : vector) */
	map(0x004000, 0x007fff).readonly().nopw().share("sharedram3");     /* shadow ($fe4000 - $fe7fff : read only) */
	map(0x400000, 0x400003).noprw();
	map(0x800000, 0x800003).nopr().w("ymsnd", FUNC(ym2413_device::write)).umask16(0x00ff);
	map(0x800005, 0x800005).rw("oki", FUNC(okim6295_device::read), FUNC(okim6295_device::write));
	map(0xc00000, 0xc1ffff).ram().share("sharedram1");
	map(0xfe0000, 0xfe3fff).ram().share("sharedram2");
	map(0xfe4000, 0xffffff).ram().share("sharedram3");
}

/***************************************************************************
                                Input Ports
***************************************************************************/

static INPUT_PORTS_START( hyprduel )
	PORT_START("SERVICE")
	PORT_SERVICE_NO_TOGGLE( 0x8000, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x4000, 0x0000, "Show Warning" )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT( 0x3fff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("DSW")
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x0040, 0x0000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Start Up Mode" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0100, 0x0100, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_BIT(     0x0200, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_DIPNAME( 0x0c00, 0x0c00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Easy ) )
	PORT_DIPSETTING(      0x0c00, DEF_STR( Normal ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( Hard ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Very_Hard ) )
	PORT_DIPNAME( 0x3000, 0x3000, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x2000, "2" )
	PORT_DIPSETTING(      0x3000, "3" )
	PORT_DIPSETTING(      0x1000, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_BIT(     0xc000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("P1_P2")
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT(  0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT(  0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN ) PORT_PLAYER(1)
	PORT_BIT(  0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT(  0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT(  0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT(  0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT(  0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT(  0x2000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT(  0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT(  0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN ) PORT_PLAYER(2)

	PORT_START("SYSTEM")
	PORT_BIT(  0x0001, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_IMPULSE(2)
	PORT_BIT(  0x0002, IP_ACTIVE_LOW, IPT_COIN2 ) PORT_IMPULSE(2)
	PORT_BIT(  0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT(  0x0008, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT(  0x0010, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT(  0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT(  0xffc0, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END

static INPUT_PORTS_START( magerror )
	PORT_INCLUDE( hyprduel )

	PORT_MODIFY("DSW")
	PORT_DIPNAME( 0x0080, 0x0080, "Start Up Mode" )
	PORT_DIPSETTING(      0x0080, "Game Mode" )
	PORT_DIPSETTING(      0x0000, "Test Mode" )
INPUT_PORTS_END

/***************************************************************************
                            Graphics Layouts
***************************************************************************/

/* 8x8x4 tiles */
static const gfx_layout layout_8x8x4 =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ 4*1,4*0, 4*3,4*2, 4*5,4*4, 4*7,4*6 },
	{ STEP8(0,4*8) },
	4*8*8
};

/* 8x8x8 tiles for later games */
static GFXLAYOUT_RAW( layout_8x8x8, 8, 8, 8*8, 32*8 )

/* 16x16x4 tiles for later games */
static const gfx_layout layout_16x16x4 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ 4*1,4*0, 4*3,4*2, 4*5,4*4, 4*7,4*6, 4*9,4*8, 4*11,4*10, 4*13,4*12, 4*15,4*14 },
	{ STEP16(0,4*16) },
	4*8*8
};

/* 16x16x8 tiles for later games */
static GFXLAYOUT_RAW( layout_16x16x8, 16, 16, 16*8, 32*8 )

static GFXDECODE_START( i4220 )
	GFXDECODE_ENTRY( "gfx1", 0, layout_8x8x4,    0x0, 0x100 ) // [0] 4 Bit Tiles
	GFXDECODE_ENTRY( "gfx1", 0, layout_8x8x8,    0x0,  0x10 ) // [1] 8 Bit Tiles
	GFXDECODE_ENTRY( "gfx1", 0, layout_16x16x4,  0x0, 0x100 ) // [2] 4 Bit Tiles 16x16
	GFXDECODE_ENTRY( "gfx1", 0, layout_16x16x8,  0x0,  0x10 ) // [3] 8 Bit Tiles 16x16
GFXDECODE_END

/***************************************************************************
                                Machine Drivers
***************************************************************************/

void hyprduel_state::machine_reset()
{
	/* start with cpu2 halted */
	m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	m_subcpu_resetline = 1;
	m_cpu_trigger = 0;

	m_requested_int = 0x00;
	m_blitter_bit = 2;
	*m_irq_enable = 0xff;
}

MACHINE_START_MEMBER(hyprduel_state,hyprduel)
{
	save_item(NAME(m_blitter_bit));
	save_item(NAME(m_requested_int));
	save_item(NAME(m_subcpu_resetline));
	save_item(NAME(m_cpu_trigger));

	m_vblank_end_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(hyprduel_state::vblank_end_callback), this));
}

MACHINE_CONFIG_START(hyprduel_state::i4220_config)
	MCFG_DEVICE_ADD("vdp", I4220, XTAL(26'666'000))
	MCFG_I4100_GFXDECODE("gfxdecode")
	MCFG_I4100_BLITTER_END_CALLBACK(WRITELINE(hyprduel_state,vdp_blit_end_w))

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_VIDEO_ATTRIBUTES(VIDEO_UPDATE_SCANLINE)
	MCFG_SCREEN_REFRESH_RATE(60) // Unknown/Unverified
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(320, 224)
	MCFG_SCREEN_VISIBLE_AREA(0, 320-1, FIRST_VISIBLE_LINE, LAST_VISIBLE_LINE)
	MCFG_SCREEN_UPDATE_DEVICE("vdp", imagetek_i4100_device, screen_update)
	MCFG_SCREEN_PALETTE(":vdp:palette")

	MCFG_GFXDECODE_ADD("gfxdecode", ":vdp:palette", i4220)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hyprduel_state::hyprduel)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000,20000000/2)      /* 10MHz */
	MCFG_CPU_PROGRAM_MAP(hyprduel_map)
	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", hyprduel_state, interrupt, "screen", 0, 1)

	MCFG_CPU_ADD("sub", M68000,20000000/2)      /* 10MHz */
	MCFG_CPU_PROGRAM_MAP(hyprduel_map2)

	MCFG_MACHINE_START_OVERRIDE(hyprduel_state,hyprduel)

	/* video hardware */
	i4220_config(config);

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_YM2151_ADD("ymsnd", 4000000)
	MCFG_YM2151_IRQ_HANDLER(INPUTLINE("sub", 1))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.80)

	MCFG_OKIM6295_ADD("oki", 4000000/16/16*132, PIN7_HIGH) // clock frequency & pin 7 not verified
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.57)
MACHINE_CONFIG_END


MACHINE_CONFIG_START(hyprduel_state::magerror)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68000,20000000/2)      /* 10MHz */
	MCFG_CPU_PROGRAM_MAP(magerror_map)
	MCFG_TIMER_DRIVER_ADD_SCANLINE("scantimer", hyprduel_state, interrupt, "screen", 0, 1)

	MCFG_CPU_ADD("sub", M68000,20000000/2)      /* 10MHz */
	MCFG_CPU_PROGRAM_MAP(magerror_map2)
	MCFG_CPU_PERIODIC_INT_DRIVER(hyprduel_state, irq1_line_hold, 968)        /* tempo? */

	MCFG_MACHINE_START_OVERRIDE(hyprduel_state,hyprduel)

	/* video hardware */
	i4220_config(config);

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_SOUND_ADD("ymsnd", YM2413, 3579545)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	MCFG_OKIM6295_ADD("oki", 4000000/16/16*132, PIN7_HIGH) // clock frequency & pin 7 not verified
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.57)
MACHINE_CONFIG_END

/***************************************************************************
                                ROMs Loading
***************************************************************************/

ROM_START( hyprduel )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "24.u24", 0x000000, 0x40000, CRC(c7402722) SHA1(e385676cdcee65a3ddf07791d82a1fe83ba1b3e2) ) /* Also silk screened as position 10 */
	ROM_LOAD16_BYTE( "23.u23", 0x000001, 0x40000, CRC(d8297c2b) SHA1(2e23c5b1784d0a465c0c0dc3ca28505689a8b16c) ) /* Also silk screened as position  9 */

	ROM_REGION( 0x400000, "gfx1", 0 )   /* Gfx + Prg + Data (Addressable by CPU & Blitter) */
	ROMX_LOAD( "ts_hyper-1.u74", 0x000000, 0x100000, CRC(4b3b2d3c) SHA1(5e9e8ec853f71aeff3910b93dadbaeae2b61717b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-2.u75", 0x000002, 0x100000, CRC(dc230116) SHA1(a3c447657d8499764f52c81382961f425c56037b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-3.u76", 0x000004, 0x100000, CRC(2d770dd0) SHA1(27f9e7f67e96210d3710ab4f940c5d7ae13f8bbf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-4.u77", 0x000006, 0x100000, CRC(f88c6d33) SHA1(277b56df40a17d7dd9f1071b0d498635a5b783cd) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x40000, "oki", 0 ) /* Samples */
	ROM_LOAD( "97.u97", 0x00000, 0x40000, CRC(bf3f8574) SHA1(9e743f05e53256c886d43e1f0c43d7417134b9b3) ) /* Also silk screened as position 11 */
ROM_END

ROM_START( hyprduel2 )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "24a.u24", 0x000000, 0x40000, CRC(2458f91d) SHA1(c75c7bccc84738e29b35667793491a1213aea1da) ) /* Also silk screened as position 10 */
	ROM_LOAD16_BYTE( "23a.u23", 0x000001, 0x40000, CRC(98aedfca) SHA1(42028e57ac79473cde683be2100b953ff3b2b345) ) /* Also silk screened as position  9 */

	ROM_REGION( 0x400000, "gfx1", 0 )   /* Gfx + Prg + Data (Addressable by CPU & Blitter) */
	ROMX_LOAD( "ts_hyper-1.u74", 0x000000, 0x100000, CRC(4b3b2d3c) SHA1(5e9e8ec853f71aeff3910b93dadbaeae2b61717b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-2.u75", 0x000002, 0x100000, CRC(dc230116) SHA1(a3c447657d8499764f52c81382961f425c56037b) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-3.u76", 0x000004, 0x100000, CRC(2d770dd0) SHA1(27f9e7f67e96210d3710ab4f940c5d7ae13f8bbf) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "ts_hyper-4.u77", 0x000006, 0x100000, CRC(f88c6d33) SHA1(277b56df40a17d7dd9f1071b0d498635a5b783cd) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x40000, "oki", 0 ) /* Samples */
	ROM_LOAD( "97.u97", 0x00000, 0x40000, CRC(bf3f8574) SHA1(9e743f05e53256c886d43e1f0c43d7417134b9b3) ) /* Also silk screened as position 11 */
ROM_END

ROM_START( magerror )
	ROM_REGION( 0x80000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "24.u24", 0x000000, 0x40000, CRC(5e78027f) SHA1(053374942bc545a92cc6f6ab6784c4626e4ec9e1) ) /* Also silk screened as position 10 */
	ROM_LOAD16_BYTE( "23.u23", 0x000001, 0x40000, CRC(7271ec70) SHA1(bd7666390b70821f90ba976a3afe3194fb119478) ) /* Also silk screened as position  9 */

	ROM_REGION( 0x400000, "gfx1", 0 )   /* Gfx + Prg + Data (Addressable by CPU & Blitter) */
	ROMX_LOAD( "mr93046-02.u74", 0x000000, 0x100000, CRC(f7ba06fb) SHA1(e1407b0d03863f434b68183c01e8547612e5c5fd) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mr93046-04.u75", 0x000002, 0x100000, CRC(8c114d15) SHA1(4eb1f82e7992deb126633287cb4fd2a6d215346c) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mr93046-01.u76", 0x000004, 0x100000, CRC(6cc3b928) SHA1(f19d0add314867bfb7dcefe8e7a2d50a84530df7) , ROM_GROUPWORD | ROM_SKIP(6) )
	ROMX_LOAD( "mr93046-03.u77", 0x000006, 0x100000, CRC(6b1eb0ea) SHA1(6167a61562ef28147a7917c692f181f3fc2d5be6) , ROM_GROUPWORD | ROM_SKIP(6) )

	ROM_REGION( 0x40000, "oki", 0 ) /* Samples */
	ROM_LOAD( "97.u97", 0x00000, 0x40000, CRC(2e62bca8) SHA1(191fff11186dbbc1d9d9f3ba1b6e17c38a7d2d1d) ) /* Also silk screened as position 11 */
ROM_END


DRIVER_INIT_MEMBER(hyprduel_state,hyprduel)
{
	m_int_num = 0x02;

	/* cpu synchronization (severe timings) */
	m_maincpu->space(AS_PROGRAM).install_write_handler(0xc0040e, 0xc00411, write16_delegate(FUNC(hyprduel_state::hyprduel_cpusync_trigger1_w),this));
	m_subcpu->space(AS_PROGRAM).install_read_handler(0xc00408, 0xc00409, read16_delegate(FUNC(hyprduel_state::hyprduel_cpusync_trigger1_r),this));
	m_maincpu->space(AS_PROGRAM).install_write_handler(0xc00408, 0xc00409, write16_delegate(FUNC(hyprduel_state::hyprduel_cpusync_trigger2_w),this));
	m_subcpu->space(AS_PROGRAM).install_read_handler(0xfff34c, 0xfff34d, read16_delegate(FUNC(hyprduel_state::hyprduel_cpusync_trigger2_r),this));
}

DRIVER_INIT_MEMBER(hyprduel_state,magerror)
{
	m_int_num = 0x01;
}


GAME( 1993, hyprduel, 0,        hyprduel, hyprduel, hyprduel_state, hyprduel, ROT0, "Technosoft", "Hyper Duel (Japan set 1)", MACHINE_SUPPORTS_SAVE )
GAME( 1993, hyprduel2,hyprduel, hyprduel, hyprduel, hyprduel_state, hyprduel, ROT0, "Technosoft", "Hyper Duel (Japan set 2)", MACHINE_SUPPORTS_SAVE )
GAME( 1994, magerror, 0,        magerror, magerror, hyprduel_state, magerror, ROT0, "Technosoft / Jaleco", "Magical Error wo Sagase", MACHINE_SUPPORTS_SAVE )
