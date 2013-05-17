/* variable_container - handling a bunch of variables containting NUL
 *   terminated strings, allowing callbacks to be called on changes.
 *
 * upnp_last_change_collector - handling of the LastChange variable in UPnP.
 *   Collects the last changes and notifies device.
 *
 * Copyright (C) 2013 Henner Zeller
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA 02110-1301, USA.
 *
 */ 

/*
2)
 - provide a UPnP variable change collector, that reacts on callbacks and
   builds a LastChange document.
 - the collector can have a OpenTransaction(), Commit() to keep collect
   updates before it changes the LastChange. If no transaction is open, it
   changes right away. assert(): only one transaction can be open at a time.
 - (future: add rate limiting: only fire if last commit is older than x time)
 - provide a way to register callbacks when transaction commits.

[ note: the LastChange variable should probably be not stored in the original
  set to avoid recursive calls ]

3) use
 - create transport variable list with defaults and store them in variable
   container.
 - create variable change collector with proper namespace.
 - in upnp_device, handle_subscription_request(): register callback at
   upnp variable cange collector.
*/

#ifndef VARIABLE_CONTAINER_H
#define VARIABLE_CONTAINER_H

// -- VariableContainer
typedef struct variable_container variable_container_t;

// Create a new variable container. The variable_names need to be valid for the
// lifetime of this objec.
variable_container_t *VariableContainer_new(int variable_num,
					    const char **variable_names,
					    const char **variable_init_values);
void VariableContainer_delete(variable_container_t *object);

// Only temporary while refactoring.
const char **VariableContainer_get_values_hack(variable_container_t *object);

// Change content of variable with given number to NUL terminated content.
// Returns '1' if value actually changed and all callbacks were called,
// '0' if no change was detected.
int VariableContainer_change(variable_container_t *object,
			     int variable_num, const char *value);

// Callback handling. Whenever a variable changes, the callback is called.
// Be careful when changing variables in the original container as this will
// trigger recursive calls to the container.
typedef void (*variable_change_listener_t)(void *userdata,
					   int var_num, const char *var_name,
					   const char *old_value,
					   const char *new_value);
void VariableContainer_register_callback(variable_container_t *object,
					 variable_change_listener_t callback,
					 void *userdata);
// No unregister yet; needed ?

// -- UPnP LastChange collector
struct upnp_device;  // forward declare.
typedef struct upnp_last_change_collector upnp_last_change_collector_t;
upnp_last_change_collector_t *
UPnPLastChangeCollector_new(variable_container_t *variable_container,
			    int last_change_var_num,
			    struct upnp_device *upnp_device,
			    const char *service_name);

// If we know that there are a couple of changes upcoming, we can
// 'start_transaction' and tell the collector to keep collecting until we
// 'commit'. There can be only one transaction open at a time.
void UPnPLastChangeCollector_start_transaction(upnp_last_change_collector_t *o);
void UPnPLastChangeCollector_commit(upnp_last_change_collector_t *object);

// no delete yet. We leak that.
#endif  /* VARIABLE_CONTAINER_H */