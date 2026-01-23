#include "mouse.h"
#include "ps2.h"

int ps2_mouse_init(struct mouse *mouse);

struct mouse ps2_mouse = {
	.name = "PS/2 Mouse",
	.init = ps2_mouse_init,
};

struct ps2_mouse ps2_mouse_private = {0};

struct mouse *ps2_mouse_get()
{
	return &ps2_mouse;
}

int ps2_mouse_wait(int type)
{
	int res = -ETIMEOUT;
	if (type == PS2_WAIT_FOR_INPUT_TO_CLEAR)
	{
		udelay(100000);
		if ((insb(PS2_STATUS_PORT) & 2) == 0)
		{
			res = 0;
		}

		return res;
	}

	udelay(100000);
	if (insb(PS2_STATUS_PORT) & 1)
	{
		res = 0;
	}

	return res;
}

void ps2_mouse_write(uint8_t byte)
{
	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	outb(PS2_STATUS_PORT, PS2_WRITE_TO_MOUSE);
	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	outb(PS2_COMMUNICATION_PORT, byte);
}

void ps2_mouse_handle_interrupt(struct interrupt_frame *frame)
{
	// static because we want to keep the packet data between interrupts
	static uint8_t packet[4];
	static int packet_byte_count = 0;
	size_t ps2_mouse_packet_size = ps2_mouse_private.mouse_packet_size;
	uint8_t data = insb(PS2_COMMUNICATION_PORT);
	if (packet_byte_count == 0 && !(data & 0x08))
	{
		// out of sync, discard byte
		return;
	}

	// save incoming byte
	packet[packet_byte_count++] = data;
	if (packet_byte_count < ps2_mouse_packet_size)
	{
		return;
	}

	// complete packet received, process it
	packet_byte_count = 0;
	int8_t dx = (int8_t)packet[1];
	int8_t dy = (int8_t)packet[2];

	// invert dy to match screen coordinates
	dy = -dy;

	uint8_t left_button = packet[0] & 0x1;
	uint8_t right_button = (packet[0] >> 1) & 0x1;
	uint8_t middle_button = (packet[0] >> 2) & 0x1;

	int8_t scroll = 0;
	if (ps2_mouse_packet_size == 4)
	{
		scroll = (int8_t)packet[3];
	}

	if (scroll && left_button && right_button && middle_button)
	{
		// supress warning
	}

	int x_result = (int)ps2_mouse.coords.x + (int)dx;
	int y_result = (int)ps2_mouse.coords.y + (int)dy;
	struct graphics_info *screen_graphics = graphics_screen_info();
	if (x_result < 0)
	{
		x_result = 0;
	}

	if (y_result < 0)
	{
		y_result = 0;
	}

	if (x_result > (int)(screen_graphics->width - ps2_mouse.graphic.width))
	{
		x_result = screen_graphics->width - ps2_mouse.graphic.width;
	}

	if (y_result > (int)(screen_graphics->height - ps2_mouse.graphic.height))
	{
		y_result = screen_graphics->height - ps2_mouse.graphic.height;
	}

	mouse_position_set(&ps2_mouse, (size_t)x_result, (size_t)y_result);

	MOUSE_CLICK_TYPE click_type = MOUSE_NO_CLICK;
	if (left_button)
	{
		click_type = MOUSE_LEFT_BUTTON_CLICK;
	}
	else if (right_button)
	{
		click_type = MOUSE_RIGHT_BUTTON_CLICK;
	}
	else if (middle_button)
	{
		click_type = MOUSE_MIDDLE_BUTTON_CLICK;
	}

	if (click_type != MOUSE_NO_CLICK)
	{
		mouse_click(&ps2_mouse, click_type);
	}

	mouse_move(&ps2_mouse);
}

int ps2_mouse_init(struct mouse *mouse)
{
	int res = 0;
	idt_register_interrupt_callback(ISR_MOUSE_INTERRUPT, ps2_mouse_handle_interrupt);
	irq_enable(IRQ_PS2_MOUSE);

	ps2_mouse_wait(PS2_WAIT_FOR_INPUT_TO_CLEAR);
	outb(PS2_STATUS_PORT, PS2_COMMAND_ENABLE_SECOND_PORT);

	ps2_mouse_wait(PS2_WAIT_FOR_INPUT_TO_CLEAR);
	outb(PS2_STATUS_PORT, PS2_READ_CONFIG_BYTE);

	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	uint8_t config_byte = insb(PS2_COMMUNICATION_PORT);
	config_byte &= ~(1 << 5); // clear bit 5 to enable clocking on mouse interrupts
	config_byte |= (1 << 1);  // set bit 1 to enable mouse interrupts
	ps2_mouse_wait(PS2_WAIT_FOR_INPUT_TO_CLEAR);
	outb(PS2_STATUS_PORT, PS2_UPDATE_CONFIG_BYTE);
	outb(PS2_COMMUNICATION_PORT, config_byte);

	ps2_mouse_write(PS2_MOUSE_RESET);

	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	uint8_t ack = insb(PS2_COMMUNICATION_PORT);
	if (ack != PS2_MOUSE_ACK)
	{
		print("ps2_mouse_init: no ACK after mouse reset\n");
		res = -EIO;
		goto out;
	}

	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	uint8_t self_test = insb(PS2_COMMUNICATION_PORT);
	if (self_test != PS2_MOUSE_SELF_TEST_PASS)
	{
		print("ps2_mouse_init: mouse self-test failed\n");
		res = -EIO;
		goto out;
	}

	ps2_mouse_write(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	uint8_t device_id = insb(PS2_COMMUNICATION_PORT);
	if (device_id)
	{
		print("ps2_mouse_init: mouse detected\n");
	}

	ps2_mouse_write(PS2_MOUSE_ENABLE_PACKET_STREAMING);
	ps2_mouse_wait(PS2_WAIT_FOR_OUTPUT_TO_BE_SET);
	ack = insb(PS2_COMMUNICATION_PORT);
	if (ack != PS2_MOUSE_ACK)
	{
		print("ps2_mouse_init: no ACK after enabling packet streaming\n");
		res = -EIO;
		goto out;
	}

	mouse->private = &ps2_mouse_private;
	ps2_mouse_private.device_id = device_id;
	ps2_mouse_private.mouse_packet_size = 3;
	if (device_id == PS2_SCROLL_WHEEL_MOUSE_DEVICE_ID)
	{
		ps2_mouse_private.mouse_packet_size = 4;
	}

out:
	return res;
}