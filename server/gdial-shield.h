#ifndef GDIAL_SHIELD_H_
#define GDIAL_SHIELD_H_

#include <libsoup/soup.h>
#include "gdial-config.h"

void gdial_shield_init(void);
void gdial_shield_server(SoupServer *server);
void gdial_shield_term(void);

#endif
