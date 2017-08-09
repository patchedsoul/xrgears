#pragma once

#include <xcb/xcb.h>
#include <string.h>
#include <stdio.h>

#include "common.h"

#include "silo.h"

// XCB
static xcb_atom_t
get_atom(struct xcb_connection_t *conn, const char *name)
{
   xcb_intern_atom_cookie_t cookie;
   xcb_intern_atom_reply_t *reply;
   xcb_atom_t atom;

   cookie = xcb_intern_atom(conn, 0, strlen(name), name);
   reply = xcb_intern_atom_reply(conn, cookie, NULL);
   if (reply)
      atom = reply->atom;
   else
      atom = XCB_NONE;

   free(reply);
   return atom;
}

// Return -1 on failure.
int
init_xcb(struct vkcube *vc)
{
   xcb_screen_iterator_t iter;
   static const char title[] = "Vulkan Cube";

   vc->xcb.conn = xcb_connect(0, 0);
   if (xcb_connection_has_error(vc->xcb.conn))
      return -1;

   vc->xcb.window = xcb_generate_id(vc->xcb.conn);

   uint32_t window_values[] = {
      XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_KEY_PRESS
   };

   iter = xcb_setup_roots_iterator(xcb_get_setup(vc->xcb.conn));

   xcb_create_window(vc->xcb.conn,
                     XCB_COPY_FROM_PARENT,
                     vc->xcb.window,
                     iter.data->root,
                     0, 0,
                     vc->width,
                     vc->height,
                     0,
                     XCB_WINDOW_CLASS_INPUT_OUTPUT,
                     iter.data->root_visual,
                     XCB_CW_EVENT_MASK, window_values);

   vc->xcb.atom_wm_protocols = get_atom(vc->xcb.conn, "WM_PROTOCOLS");
   vc->xcb.atom_wm_delete_window = get_atom(vc->xcb.conn, "WM_DELETE_WINDOW");
   xcb_change_property(vc->xcb.conn,
                       XCB_PROP_MODE_REPLACE,
                       vc->xcb.window,
                       vc->xcb.atom_wm_protocols,
                       XCB_ATOM_ATOM,
                       32,
                       1, &vc->xcb.atom_wm_delete_window);

   xcb_change_property(vc->xcb.conn,
                       XCB_PROP_MODE_REPLACE,
                       vc->xcb.window,
                       get_atom(vc->xcb.conn, "_NET_WM_NAME"),
                       get_atom(vc->xcb.conn, "UTF8_STRING"),
                       8, // sizeof(char),
                       strlen(title), title);

   xcb_map_window(vc->xcb.conn, vc->xcb.window);

   xcb_flush(vc->xcb.conn);

   init_vk(vc, VK_KHR_XCB_SURFACE_EXTENSION_NAME);

   if (!vkGetPhysicalDeviceXcbPresentationSupportKHR(vc->physical_device, 0,
                                                     vc->xcb.conn,
                                                     iter.data->root_visual)) {
      fail("Vulkan not supported on given X window");
   }

   VkXcbSurfaceCreateInfoKHR surfaceInfo = {};

   surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
   surfaceInfo.connection = vc->xcb.conn;
   surfaceInfo.window = vc->xcb.window;

   vkCreateXcbSurfaceKHR(vc->instance, &surfaceInfo, NULL, &vc->surface);

   vc->image_format = choose_surface_format(vc);

   init_vk_objects(vc);

   vc->image_count = 0;

   return 0;
}

static void
schedule_xcb_repaint(struct vkcube *vc)
{
   xcb_client_message_event_t client_message;

   client_message.response_type = XCB_CLIENT_MESSAGE;
   client_message.format = 32;
   client_message.window = vc->xcb.window;
   client_message.type = XCB_ATOM_NOTICE;

   xcb_send_event(vc->xcb.conn, 0, vc->xcb.window,
                  0, (char *) &client_message);
}

void
mainloop_xcb(struct vkcube *vc)
{
   xcb_generic_event_t *event;
   xcb_key_press_event_t *key_press;
   xcb_client_message_event_t *client_message;
   xcb_configure_notify_event_t *configure;

   while (1) {
      bool repaint = false;
      event = xcb_wait_for_event(vc->xcb.conn);
      while (event) {
	 switch (event->response_type & 0x7f) {
	 case XCB_CLIENT_MESSAGE:
		client_message = (xcb_client_message_event_t *) event;
	    if (client_message->window != vc->xcb.window)
		break;

	    if (client_message->type == vc->xcb.atom_wm_protocols &&
	        client_message->data.data32[0] == vc->xcb.atom_wm_delete_window) {
		exit(0);
	    }

	    if (client_message->type == XCB_ATOM_NOTICE)
		repaint = true;
		break;

	 case XCB_CONFIGURE_NOTIFY:
		configure = (xcb_configure_notify_event_t *) event;
	    if (vc->width != configure->width ||
	        vc->height != configure->height) {
		if (vc->image_count > 0) {
		  vkDestroySwapchainKHR(vc->device, vc->swap_chain, NULL);
		  vc->image_count = 0;
	       }

	       vc->width = configure->width;
	       vc->height = configure->height;
	    }
		break;

	 case XCB_EXPOSE:
		schedule_xcb_repaint(vc);
		break;

	 case XCB_KEY_PRESS:
		key_press = (xcb_key_press_event_t *) event;

	    if (key_press->detail == 9)
		exit(0);

		break;
	 }
	 free(event);

	 event = xcb_poll_for_event(vc->xcb.conn);
      }

      if (repaint) {
	 if (vc->image_count == 0)
	    create_swapchain(vc);

	 uint32_t index;
	 vkAcquireNextImageKHR(vc->device, vc->swap_chain, 60,
	                       vc->semaphore, VK_NULL_HANDLE, &index);

	 vc->model.render(vc, &vc->buffers[index]);

	 VkResult result;

	 uint32_t indices[] = { index, };

	 VkSwapchainKHR swapchains[] = { vc->swap_chain, };

	 VkPresentInfoKHR presentInfo = {};
	 presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	 presentInfo.swapchainCount = 1;
	 presentInfo.pSwapchains = swapchains;
	 presentInfo.pImageIndices = indices;
	 presentInfo.pResults = &result;


	 vkQueuePresentKHR(vc->queue, &presentInfo);

	 schedule_xcb_repaint(vc);
      }

      xcb_flush(vc->xcb.conn);
   }
}
