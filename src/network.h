#include "config.h"
int configure_network_interface(const struct config *cfg);
int enable_network_interface(const char *ifname);
int disable_network_interface(const char *ifname);
int enable_ping_response();
int disable_ping_response();


