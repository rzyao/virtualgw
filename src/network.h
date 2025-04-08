// 假设你未来会放到 network.c 里
int configure_network_interface(const struct detector_config *cfg);
int enable_virtual_interface();
int disable_virtual_interface();
int check_device_status(struct detector_config *cfg);
