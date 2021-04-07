#ifndef _JACK_RT5665_SYSFS_CB_H
#define _JACK_RT5665_SYSFS_CB_H

extern int rt5665_jack_det;
extern int rt5665_ear_mic;

void register_rt5665_jack_cb(struct snd_soc_codec *codec);

#endif /*_JACK_RT5665_SYSFS_CB_H */
