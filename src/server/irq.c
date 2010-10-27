#include "remote-control.h"

int32_t medcom_irq_enable(void *priv, uint8_t virtkey)
{
	return -ENOSYS;
}

int32_t medcom_irq_get_mask(void *priv, uint32_t *mask)
{
	return -ENOSYS;
}

int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info)
{
	return -ENOSYS;
}
