#ifndef __WM8994_INCALL_BOOST_H_
#define __WM8994_INCALL_BOOST_H_

// wm8994_aries.c
extern unsigned short incall_boost_rcv;
extern unsigned short incall_boost_bt;
extern unsigned short incall_boost_spk;
extern unsigned short incall_boost_hp;

// wm8994_incall_boost.c
void incall_boost_hook_wm8994_pcm_probe(struct snd_soc_codec *codec);

#endif // __WM8994_VOLUMEBOOST_H_

