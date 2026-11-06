#ifndef PIDOSCFG_LIVE_H
#define PIDOSCFG_LIVE_H
void live_update(int mixer_page);
void live_cancel(void);
void live_commit(void);
const char *live_status(void);
#endif
