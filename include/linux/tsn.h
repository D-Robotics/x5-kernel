#ifndef _TSN_H
#define _TSN_H
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/configfs.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/module.h>


enum avtp_subtype {
	TSN_61883_IIDC = 0,	/* IEC 61883/IIDC Format */
	TSN_MMA_STREAM,		/* MMA Streams */
	TSN_AAF,		/* AVTP Audio Format */
	TSN_CVF,		/* Compressed Video Format */
	TSN_CRF,		/* Clock Reference Format */
	TSN_TSCF,		/* Time-Synchronous Control Format */
	TSN_SVF,		/* SDI Video Format */
	TSN_RVF,		/* Raw Video Format */
	/* 0x08 - 0x6D reserved */
	TSN_AEF_CONTINOUS = 0x6e, /* AES Encrypted Format Continous */
	TSN_VSF_STREAM,		/* Vendor Specific Format Stream */
	/* 0x70 - 0x7e reserved */
	TSN_EF_STREAM = 0x7f,	/* Experimental Format Stream */
	/* 0x80 - 0x81 reserved */
	TSN_NTSCF = 0x82,	/* Non Time-Synchronous Control Format */
	/* 0x83 - 0xed reserved */
	TSN_ESCF = 0xec,	/* ECC Signed Control Format */
	TSN_EECF,		/* ECC Encrypted Control Format */
	TSN_AEF_DISCRETE,	/* AES Encrypted Format Discrete */
	/* 0xef - 0xf9 reserved */
	TSN_ADP = 0xfa,		/* AVDECC Discovery Protocol */
	TSN_AECP,		/* AVDECC Enumeration and Control Protocol */
	TSN_ACMP,		/* AVDECC Connection Management Protocol */
	/* 0xfd reserved */
	TSN_MAAP = 0xfe,	/* MAAP Protocol */
	TSN_EF_CONTROL,		/* Experimental Format Control */
};


/* Link-states to help error-recovery detected from irq context.
 */
enum link_states {
	LINK_OFF = 0,
	LINK_RUNNING,
	LINK_ERROR,
};


/* Common part of avtph header
 *
 * AVB Transport Protocol Common Header
 *
 * Defined in 1722-2011 Sec. 5.2
 */
struct avtp_ch {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	/* use avtp_subtype enum.
	 */
	u8 subtype:7;

	/* Controlframe: 1
	 * Dataframe   : 0
	 */
	u8 cd:1;

	/* Type specific data, part 1 */
	u8 tsd_1:4;

	/* In current version of AVB, only 0 is valid, all other values
	 * are reserved for future versions.
	 */
	u8 version:3;

	/* Valid StreamID in frame
	 *
	 * ControlData not related to a specific stream should clear
	 * this (and have stream_id = 0), _all_ other values should set
	 * this to 1.
	 */
	u8 sv:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u8 cd:1;
	u8 subtype:7;
	u8 sv:1;
	u8 version:3;
	u8 tsd_1:4;
#else
#error "Unknown Endianness, cannot determine bitfield ordering"
#endif
	/* Type specific data (adjacent to tsd_1, but split due to bitfield) */
	u16 tsd_2;
	u64 stream_id;

	/*
	 * payload by subtype
	 */
	u8 pbs[0];
} __packed;


/* AVTPDU Common Control header format
 * IEEE 1722#5.3
 */
struct avtpc_header {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8 subtype:7;
	u8 cd:1;
	u8 control_data:4;
	u8 version:3;
	u8 sv:1;
	u16 control_data_length:11;
	u16 status:5;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u8 cd:1;
	u8 subtype:7;
	u8 sv:1;
	u8 version:3;
	u8 control_data:4;
	u16 status:5;
	u16 control_data_length:11;
#else
#error "Unknown Endianness, cannot determine bitfield ordering"
#endif
	u64 stream_id;
} __packed;



/* AVTP common stream data AVTPDU header format
 * IEEE 1722#5.4
 */
struct avtpdu_header {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	u8 subtype:7;
	u8 cd:1;

	/* avtp_timestamp valid */
	u8 tv: 1;

	/* gateway_info valid */
	u8 gv:1;

	/* reserved */
	u8 r:1;

	/*
	 * Media clock Restart toggle
	 */
	u8 mr:1;

	u8 version:3;

	/* StreamID valid */
	u8 sv:1;
	u8 seqnr;

	/* Timestamp uncertain */
	u8 tu:1;
	u8 r2:7;
#elif defined(__BIG_ENDIAN_BITFIELD)
	u8 cd:1;
	u8 subtype:7;

	u8 sv:1;
	u8 version:3;
	u8 mr:1;
	u8 r:1;
	u8 gv:1;
	u8 tv: 1;

	u8 seqnr;
	u8 r2:7;
	u8 tu:1;
#else
#error "Unknown Endianness, cannot determine bitfield ordering"
#endif

	u64 stream_id;

	u32 avtp_timestamp;
	u32 gateway_info;

	/* Stream Data Length */
	u16 sd_len;

	/* Protocol specific header, derived from avtp_subtype */
	u16 psh;

	/* Stream Payload Data 0 to n octets
	 * n so that total size < MTU
	 */
	u8 data[0];
} __packed;



struct tsn_list {
	struct list_head head;
	spinlock_t lock;
	struct configfs_subsystem tsn_subsys;

	/*
	 * TSN-timer is running. Not to be confused with the per-link
	 * disabled flag which indicates if a remote client, like aplay,
	 * is pushing data to it.
	 */
	atomic_t running;
	struct hrtimer tsn_timer;
	unsigned int period_ns;

	struct task_struct *tsn_thread;
	int should_run;

	size_t num_avail;
};


static inline void tsn_list_lock(struct tsn_list *list)
{
	spin_lock(&list->lock);
}
static inline void tsn_list_unlock(struct tsn_list *list)
{
	spin_unlock(&list->lock);
}


struct tsn_nic {
	struct list_head list;
	struct config_group group;
	struct net_device *dev;
	struct tsn_list *tsn_list;

	size_t dma_size;
	dma_addr_t dma_handle;
	void *dma_mem;

	char *name;
	int txq;
	u8 rx_registered:1;
	u8 capable:1;

	/*
	 * Any AVTP data stream must set the 802.1Q vlan id and priority
	 * Code point. This should be obtained from MSRP, default values
	 * are:
	 *
	 * pcp: Class A: 3
	 *	Class B: 2
	 *
	 * See IEEE 802.1Q-2011, Sec 35.2.2.9.3 and table 6-6 in 6.6.2
	 * for details.
	 */
	u8 pcp_a:3;
	u8 pcp_b:3;
};


struct tsn_shim_ops;

struct tsn_link {
	/* Locks for protecting the link
	 *
	 * Due to how we do Rx and Tx, we need different types of locks
	 * in these settings. A link _cannot_ be both, so even though
	 * this way of doing it is ugly, it should be safe.
	 *
	 * Reader: must disable interrupt as we take the lock in rx-handler (Network bh)
	 * Talker: must not disable interrupt
	 */
	spinlock_t tlock;
	raw_spinlock_t llock;
	unsigned long lflags;

	struct config_group group;
	struct tsn_nic *nic;
	struct hlist_node node;

	bool is_synced;

	/* The link itself is active, and the tsn_core will treat it as
	 * an active participant and feed data from it to the
	 * network. This places some restrictions on what attributes
	 * (most actually) that can be changed.
	 *
	 */
	atomic_t link_state;

	/* keep track of how many frames we have sent (for debugging) */
	u64 frames_sent;

	/* timestamp for last frame going in/out over the network, delta
	 * and avg delta
	 */
	u64 ts_net_ns;
	u64 ts_delta_ns;

	/* simple, exponential smoothing of the time between received
	 * samples. This is useful for shims that need to calculate an
	 * offset into the buffer of received data.
	 * exp_avg = alpha * ts_delta_ns + (1-alpha)exp_avg_{-1}
	 * avg_delta_ns = ts_delta_ns * alpha_scale + avg_delta_ns * (1 - alpha_scale)
	 */
	u64 ts_exp_avg;

	/* alpha_scale is currently in the range 0 - (2^14 - 1) because
	 * 16384 different values is "probably enough" for a smoothed
	 * avg.
	 */
	u16 ts_exp_alpha;

	/* Pointer to media-specific data.
	 * e.g. struct avb_chip
	 */
	void *media_chip;

	u64 stream_id;

	/*
	 * The max required size for a _single_ TSN frame.
	 *
	 * To be used instead of channels and sample_freq.
	 */
	u16 max_payload_size;
	u16 shim_header_size;

	/*
	 * Size of buffer (in bytes) to use when handling data to/from
	 * NIC.
	 *
	 * Smaller size will result in client being called more often
	 * but also provides lower latencies.
	 */
	size_t buffer_size;
	size_t used_buffer_size;
	size_t available_bytes;

	/* used to keep skb when we overproduce so that we can do a
	 * somewhat sane backoff.
	 */
	struct sk_buff *old_skb;

	/*
	 * Used when frames are constructed and shipped to the network
	 * layer. If this is true, 0-frames will be sent insted of data
	 * from the buffer.
	 */
	atomic_t buffer_active;

	/*
	 * ringbuffer for incoming or outging traffic
	 * +-----------------------------------+
	 * |                  ##########       |
	 * +-----------------------------------+
	 * ^                  ^         ^      ^
	 * buffer           tail      head    end
	 *
	 * Buffer: start of memory area
	 * tail: first byte of data in buffer
	 * head: first unused slot in which to store new data
	 *
	 * head,tail is used to represent the position of 'live data' in
	 * the buffer.
	 */
	void *buffer;
	void *head;
	void *tail;
	void *end;

	/* Number of bytes to run refill/drain callbacks */
	size_t low_water_mark;
	size_t high_water_mark;

	/*
	 * callback ops.
	 */
	struct tsn_shim_ops *ops;

	/*
	 * EndStation Type
	 *
	 * Either Talker or Listener
	 *
	 * 1: We are *Talker*, i.e. producing data to send
	 * 0: We are *Listener*, i.e. we receive data from another ES.
	 *
	 * This is for a single link, so even though an end-station can
	 * be both Talker *and* Listener, a link can only be one.
	 */
	u8 estype_talker;

	/*
	 * Link will use buffer managed by the shim. For this to work,
	 * the shim must:
	 *
	 * - call tsn_use_external_buffer(link, size);
	 * - provide tsn_shim_buffer_swap(link) in tsn_shim_ops
	 */
	u8 external_buffer;

	u8 last_seqnr;

	/*
	 * Class can be of different classes, currently A or B
	 *
	 * ClassA: every 125us
	 * ClassB: every 250us
	 *
	 * This will also affect how large each frame will be and will
	 * also grab the PCP from the NIC-struct
	 */
	enum sr_class class;

	u16 vlan_id;

	u8 remote_mac[6];
};

static inline void tsn_lock(struct tsn_link *link)
{
	if (link->estype_talker)
		spin_lock(&link->tlock);
	else
		raw_spin_lock_irqsave(&link->llock, link->lflags);
}

static inline void tsn_unlock(struct tsn_link *link)
{
	if (link->estype_talker)
		spin_unlock(&link->tlock);
	else
		raw_spin_unlock_irqrestore(&link->llock, link->lflags);
}

void tsn_lock_init(struct tsn_link *link);
static inline void tsn_link_on(struct tsn_link *link)
{
	if (link)
		atomic_set(&link->link_state, LINK_RUNNING);
}

static inline void tsn_link_off(struct tsn_link *link)
{
	if (link)
		atomic_set(&link->link_state, LINK_OFF);
}

static inline int tsn_link_is_off(struct tsn_link *link)
{
	if (link)
		return atomic_read(&link->link_state) == LINK_OFF;
	return 0;
}



static inline void tsn_link_err(struct tsn_link *link)
{
	if (link)
		atomic_set(&link->link_state, LINK_ERROR);
}
static inline int tsn_link_is_err(struct tsn_link *link)
{
	if (link)
		return atomic_read(&link->link_state) == LINK_ERROR;
	return 0;
}

static inline int tsn_link_is_on(struct tsn_link *link)
{
	if (link)
		return atomic_read(&link->link_state) == LINK_RUNNING;
	return 0;
}

int tsn_set_buffer_size(struct tsn_link *link, size_t bsize);
int tsn_clear_buffer_size(struct tsn_link *link);
int tsn_buffer_write(struct tsn_link *link, void *src, size_t bytes);

int tsn_buffer_read(struct tsn_link *link, void *buffer, size_t bytes);

static inline void tsn_lb_enable(struct tsn_link *link)
{
	if (link) {
		link->is_synced = false;
		link->available_bytes = 0;
		atomic_set(&link->buffer_active, 1);
	}
}

static inline void tsn_lb_disable(struct tsn_link *link)
{
	if (link) {
		atomic_set(&link->buffer_active, 0);
		link->is_synced = false;
		link->available_bytes = 0;
	}
}

static inline int tsn_lb(struct tsn_link *link)
{
	if (link)
		return atomic_read(&link->buffer_active);

	/* if link is NULL; buffer not active */
	return 0;
}

int tsn_update_net_time(struct tsn_link *link, u64 time_ns, int increment);

#define SHIM_NAME_SIZE 32
struct tsn_shim_ops {

	/* internal linked list used by tsn_core to keep track of all
	 * shims.
	 */
	struct list_head head;

	/**
	 * name - a unique name identifying this shim
	 *
	 * This is what userspace use to indicate to core what SHIM a
	 * particular link will use. If the name is already present,
	 * core will reject this name.
	 */
	char shim_name[SHIM_NAME_SIZE];

	/**
	 * probe - callback when a new link of this type is instantiated.
	 *
	 * When a new link is brought online, this is called once the
	 * essential parts of tsn_core has finiesh. Once probe_cb has
	 * finisehd, the shim _must_ be ready to accept data to/from
	 * tsn_core. On the other hand, due to the final steps of setup,
	 * it cannot expect to be called into action immediately after
	 * probe has finished.
	 *
	 * In other words, shim must be ready, but core doesn't have to
	 *
	 * @param : a particular link to pass along to the probe-function.
	 */
	int (*probe)(struct tsn_link *link);

	/**
	 * buffer_swap - set a new buffer for the link. [OPTIONAL]
	 *
	 * Used when external buffering is enabled.
	 *
	 * When called, a new buffer must be returned WITHOUT blocking
	 * as this will be called from interrupt context.
	 *
	 * The buffer returned from the shim must be at least the size
	 * of used_buffer_size.
	 *
	 * @param current link
	 * @param old_buffer the buffer that are no longer needed
	 * @param used number of bytes in buffer that has been filled with data.
	 * @return new buffer to use
	 */
	void * (*buffer_swap)(struct tsn_link *link, void *old_buffer,
			      size_t used);

	/**
	 * buffer_refill - signal shim that more data is required
	 * @link Active link
	 *
	 * This function should not do anything that can preempt the
	 * task (kmalloc, sleeping lock) or invoke actions that can take
	 * a long time to complete.
	 *
	 * This will be called from tsn_buffer_read_net() when available
	 * data in the buffer drops below low_water_mark. It will be
	 * called with the link-lock *held*
	 */
	size_t (*buffer_refill)(struct tsn_link *link);

	/**
	 * buffer_drain - shim need to copy data from buffer
	 *
	 * This will be called from tsn_buffer_write_net() when data in
	 * the buffer exceeds high_water_mark.
	 *
	 * The expected behavior is for the shim to then fill data into
	 * the buffer via tsn_buffer_write()
	 */
	size_t (*buffer_drain)(struct tsn_link *link);

	/**
	 * media_close - shut down media controller properly
	 *
	 * when the link is closed/removed for some reason
	 * external to the media controller (ALSA soundcard, v4l2 driver
	 * etc), we call this to clean up.
	 *
	 * Normal operation is stopped before media_close is called, but
	 * all references should be valid. TSN core expects media_close
	 * to handle any local cleanup, once returned, any references in
	 * stale tsn_links cannot be trusted.
	 *
	 * @link: current link where data is stored
	 * @returns: 0 upon success, negative on error.
	 */
	int (*media_close)(struct tsn_link *link);

	/**
	 * hdr_size - ask shim how large the header is
	 *
	 * Needed when reserving space in skb for transmitting data.
	 *
	 * @link: current link where data is stored
	 * @return: size of header for this shim
	 */
	size_t (*hdr_size)(struct tsn_link *link);

	/**
	 * copy_size - ask client how much from the buffer to include in
	 *	       the next frame.
	 *
	 *	       This is for *outgoing* frames, incoming frames
	 *	       have 'sd_len' set in the header.
	 *
	 *	       Note: copy_size should not return a size larger
	 *		     than link->max_payload_size
	 */
	size_t (*copy_size)(struct tsn_link *link);

	/**
	 * copy_done - signal client that a full frame was sent. This is
	 * 	       useful when a full frame needs multiple packets to be
	 * 	       sent.
	 */
	void (*copy_done)(struct tsn_link *link);

	/**
	 * validate_header - let the shim validate subtype-header
	 *
	 * Both psh and data may (or may not) contain headers that need
	 * validating. This is the responsibility of the shim to
	 * validate, and ops->valdiate_header() will be called before
	 * any data is copied from the incoming frame and into the
	 * buffer.
	 *
	 * Important: tsn_core expects validate_header to _not_ alter
	 * the contents of the frame, and ideally, validate_header could
	 * be called multiple times and give the same result.
	 *
	 * @param: active link owning the new data
	 * @param: start of data-unit header
	 *
	 * This function will be called from interrupt-context and MUST
	 * NOT take any locks.
	 */
	int (*validate_header)(struct tsn_link *link,
			       struct avtpdu_header *header);

	/**
	 * assemble_header - add shim-specific headers
	 *
	 * This adds the headers required by the current shim after the
	 * generic 1722-header.
	 *
	 * @param: active link
	 * @param: start of data-unit header
	 * @param: size of data to send in this frame
	 * @return void
	 */
	void (*assemble_header)(struct tsn_link *link,
				struct avtpdu_header *header, size_t bytes);

	/**
	 * get_payload_data - get a pointer to where the data is stored
	 *
	 * core will use the pointer (or drop it if NULL is returned)
	 * and copy header->sd_len bytes of *consecutive* data from the
	 * target memory and into the buffer memory.
	 *
	 * This is called with relevant locks held, from interrupt context.
	 *
	 * @param link active link
	 * @param header header of frame, which contains data
	 * @returns pointer to memory to copy from
	 */
	void * (*get_payload_data)(struct tsn_link *link,
				   struct avtpdu_header *header);
};



int tsn_shim_register_ops(struct tsn_shim_ops *shim_ops);
void tsn_shim_deregister_ops(struct tsn_shim_ops *shim_ops);
char *tsn_shim_get_active(struct tsn_link *link);
struct tsn_shim_ops *tsn_shim_find_by_name(const char *name);
ssize_t tsn_shim_export_probe_triggers(char *page);

static inline size_t tsn_shim_get_framesize(struct tsn_link *link)
{
	return link->ops->copy_size(link);
}

static inline size_t tsn_shim_get_hdr_size(struct tsn_link *link)
{
	size_t ret;

	if (!link || !link->ops->hdr_size)
		return -EINVAL;
	ret = link->ops->hdr_size(link);
	if (ret > link->max_payload_size)
		return -EINVAL;
	return ret;
}

static inline u8 sr_class_to_pcp(struct tsn_nic *nic, enum sr_class class)
{
	if (!nic)
		return 0;
	switch (class) {
	case SR_CLASS_A:
		return nic->pcp_a;
	case SR_CLASS_B:
		return nic->pcp_b;
		/* room for future class C & D */
	default:
		pr_err("Unknown class in mapping %d\n", class);
	}
	return 0;
}

#define module_tsn_driver(__driver_ops) \
static int __init __driver_ops##_init(void) \
{ \
	return tsn_shim_register_ops(&__driver_ops); \
} \
module_init(__driver_ops##_init); \
static void __exit __driver_ops##_exit(void) \
{ \
	tsn_shim_deregister_ops(&__driver_ops); \
} \
module_exit(__driver_ops##_exit);

#endif	/* _TSN_H */
