/* SPDX-License-Identifier: MIT */
/*
 * PhantomFPGA Application - YOUR IMPLEMENTATION
 *
 * This is the file you need to edit. Implement the TODO methods below
 * to make the app receive frames from the kernel driver and stream
 * them over TCP to the viewer.
 *
 * Read phantomfpga_app.h for the class interface and available members.
 * Read phantomfpga_uapi.h for ioctl definitions and data structures.
 *
 * Available members from the base class (PhantomFpgaApp):
 *   dev_fd_       :  FileDescriptor for the device (you set this in open_device)
 *   buffer_pool_  :  MappedMemory for the DMA buffers (you set this in setup_mmap)
 *   config_       :  AppConfig with parsed CLI parameters
 *   stats_        :  AppStats for your counters
 *   tcp_server_   :  TcpServer (may be nullptr if --tcp-server wasn't used)
 *   running_      :  volatile bool, goes false on Ctrl+C
 *
 * Utility classes:
 *   CRC32::compute(data, len)  :  IEEE 802.3 CRC32, returns uint32_t
 *   FileDescriptor(fd)         :  RAII file descriptor, use std::move()
 *   MappedMemory(addr, size)   :  RAII mmap wrapper, use std::move()
 */

#include "phantomfpga_app.h"

#include <cerrno>
#include <poll.h>
#include <sys/ioctl.h>

/* ----------------------------------------------------------------------- */
/* PhantomFpgaAppImpl: YOUR CODE GOES HERE                                 */
/*                                                                         */
/* Implement all the TODO methods below to make the app work.              */
/* The base class handles everything else (CLI, TCP, signals, cleanup).    */
/* ----------------------------------------------------------------------- */
static uint32_t read_le32(const void *ptr)
{
	const uint8_t *p = static_cast<const uint8_t *>(ptr);

	return ((uint32_t)p[0]) |
		   ((uint32_t)p[1] << 8) |
		   ((uint32_t)p[2] << 16) |
		   ((uint32_t)p[3] << 24);
}

static size_t page_align_size(size_t value)
{
	long page_size = sysconf(_SC_PAGESIZE);

	if (page_size <= 0)
		page_size = 4096;

	size_t align = static_cast<size_t>(page_size);

	return (value + align - 1) & ~(align - 1);
}

class PhantomFpgaAppImpl : public PhantomFpgaApp
{
protected:
	/*
	 * Open the PhantomFPGA device node
	 */
	int open_device() override
	{
		/* Call open() with O_RDWR on DEVICE_PATH */
		int fd = ::open(DEVICE_PATH, O_RDWR);
		if (fd < 0)
		{
			return -errno;
		}

		/* Store the result in dev_fd_ using FileDescriptor */
		dev_fd_ = FileDescriptor(fd);

		return 0;
	}

	/*
	 * Configure the device
	 * Hint: Zero-init with = {} or memset to clear the reserved fields.
	 */
	int configure_device() override
	{
		/* Create a struct phantomfpga_config (zero-initialize it!) */
		struct phantomfpga_config cfg = {};

		/* Fill in desc_count and frame_rate from config_ */
		cfg.desc_count = this->config_.desc_count;
		cfg.frame_rate = this->config_.frame_rate;

		/* Set irq_coalesce_count and irq_coalesce_timeout */
		cfg.irq_coalesce_count = DEFAULT_IRQ_COUNT;
		cfg.irq_coalesce_timeout = DEFAULT_IRQ_TIMEOUT;

		/* Call ioctl */
		int ret = ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_SET_CFG, &cfg);
		if (ret < 0)
			return -errno;

		return 0;
	}

	/*
	 * Set up memory-mapped DMA buffers
	 *
	 * After this, buffer_pool_.get() points to the DMA buffer pool.
	 * Frame N starts at: (uint8_t*)buffer_pool_.get() + N * config_.buffer_size
	 */
	int setup_mmap() override
	{
		phantomfpga_buffer_info info = {};

		int ret = ::ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_GET_BUFFER_INFO, &info);
		if (ret < 0)
			return -errno;

		/*
		 * info.buffer_size is the real DMA buffer size, usually:
		 * 5120 frame bytes + 16 completion bytes = 5136.
		 *
		 * But the mmap implementation maps each buffer at a page-aligned stride.
		 * So userspace must step by PAGE_ALIGN(buffer_size), not buffer_size.
		 */
		size_t stride = page_align_size(static_cast<size_t>(info.buffer_size));
		size_t mmap_size = stride * static_cast<size_t>(info.buffer_count);

		config_.buffer_size = static_cast<uint32_t>(stride);
		config_.desc_count = static_cast<uint32_t>(info.buffer_count);

		void *addr = ::mmap(nullptr,
							mmap_size,
							PROT_READ | PROT_WRITE,
							MAP_SHARED,
							dev_fd_.get(),
							0);

		if (addr == MAP_FAILED)
			return -errno;

		buffer_pool_ = MappedMemory(addr, mmap_size);

		if (config_.verbose)
		{
			std::fprintf(stderr,
						 "[*] mmap: raw_buffer_size=%llu stride=%zu count=%llu mmap_size=%zu\n",
						 static_cast<unsigned long long>(info.buffer_size),
						 stride,
						 static_cast<unsigned long long>(info.buffer_count),
						 mmap_size);
		}

		return 0;
	}

	/*
	 * Start frame streaming
	 */
	int start_streaming() override
	{
		/* Just one ioctl call */
		int ret = ::ioctl(this->dev_fd_.get(), PHANTOMFPGA_IOCTL_START);
		if (ret < 0)
			return -errno;
		return 0;
	}

	/*
	 * Stop frame streaming
	 */
	int stop_streaming() override
	{
		/* Just one ioctl call */
		int ret = ::ioctl(this->dev_fd_.get(), PHANTOMFPGA_IOCTL_STOP);
		if (ret < 0)
			return -errno;
		return 0;
	}

	/*
	 * TODO: Main processing loop
	 *
	 * This is the heart of the application. Loop while running_ is true:
	 *
	 * 1. If tcp_server_ exists, call tcp_server_->try_accept()
	 * 2. Use poll() on dev_fd_.get() with POLLIN, timeout ~100ms
	 * 3. If poll returns data ready:
	 *    a. Read a frame: read(dev_fd_.get(), buf, PHANTOMFPGA_FRAME_SIZE)
	 *    b. Call process_frame(buf, bytes_read) for each frame
	 * 4. Optionally print periodic stats (every few seconds)
	 *
	 * Hint: Use a stack buffer for the frame:
	 *   uint8_t frame_buf[PHANTOMFPGA_FRAME_SIZE];
	 *
	 * Hint: struct pollfd pfd = { dev_fd_.get(), POLLIN, 0 };
	 *       int ret = poll(&pfd, 1, 100);
	 */
	void main_loop() override
	{
		uint32_t desc_index = 0;

		while (running_)
		{
			if (tcp_server_)
				tcp_server_->try_accept();

			struct pollfd pfd = {
				.fd = dev_fd_.get(),
				.events = POLLIN,
				.revents = 0};

			int poll_res = ::poll(&pfd, 1, 100);

			if (poll_res == 0)
				continue;

			if (poll_res < 0)
			{
				if (errno == EINTR)
					continue;

				std::fprintf(stderr, "Error polling: %s\n", strerror(errno));
				continue;
			}

			if (pfd.revents & (POLLERR | POLLNVAL))
			{
				std::fprintf(stderr, "Poll returned error revents=0x%x\n", pfd.revents);
				continue;
			}

			if (!(pfd.revents & POLLIN))
				continue;

			uint8_t frame_buf[PHANTOMFPGA_FRAME_SIZE];

			if (config_.zero_copy)
			{
				/*
				 * Zero-copy path:
				 * frame is read directly from mmap'd DMA buffer.
				 */
				const uint8_t *buffer =
					static_cast<const uint8_t *>(buffer_pool_.get()) +
					static_cast<size_t>(desc_index) * config_.buffer_size;

				const phantomfpga_completion *completion =
					reinterpret_cast<const phantomfpga_completion *>(
						buffer + PHANTOMFPGA_FRAME_SIZE);

				uint32_t completion_status = read_le32(&completion->status);
				uint32_t actual_length = read_le32(&completion->actual_length);

				if (completion_status != PHANTOMFPGA_COMPL_OK)
				{
					std::fprintf(stderr,
								 "Error: descriptor %u completion status=0x%x\n",
								 desc_index,
								 completion_status);

					::ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_CONSUME_FRAME);
					desc_index = (desc_index + 1) % config_.desc_count;
					continue;
				}

				if (actual_length != PHANTOMFPGA_FRAME_SIZE)
				{
					std::fprintf(stderr,
								 "Error: descriptor %u short frame, actual_length=%u\n",
								 desc_index,
								 actual_length);

					::ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_CONSUME_FRAME);
					desc_index = (desc_index + 1) % config_.desc_count;
					continue;
				}

				std::memcpy(frame_buf, buffer, PHANTOMFPGA_FRAME_SIZE);

				int ret = ::ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_CONSUME_FRAME);
				if (ret < 0)
				{
					std::fprintf(stderr,
								 "Error: CONSUME_FRAME failed: %s\n",
								 strerror(errno));
					continue;
				}

				desc_index = (desc_index + 1) % config_.desc_count;

				process_frame(frame_buf, PHANTOMFPGA_FRAME_SIZE);
			}
			else
			{
				/*
				 * Normal read path:
				 * driver copies one full frame to userspace.
				 */
				ssize_t n = ::read(dev_fd_.get(), frame_buf, sizeof(frame_buf));

				if (n < 0)
				{
					if (errno == EAGAIN)
						continue;

					std::fprintf(stderr,
								 "Error reading frame: %s\n",
								 strerror(errno));
					continue;
				}

				if (n != PHANTOMFPGA_FRAME_SIZE)
				{
					std::fprintf(stderr,
								 "Error reading frame: expected %u bytes, got %zd\n",
								 PHANTOMFPGA_FRAME_SIZE,
								 n);
					continue;
				}

				process_frame(frame_buf, static_cast<uint32_t>(n));
			}
		}
	}

	/*
	 * Process a single frame
	 */
	int process_frame(const void *buffer, uint32_t len) override
	{
		bool valid = validate_frame(buffer, len);

		stats_.frames_received++;

		if (valid)
			stats_.frames_valid++;

		if (config_.verbose)
		{
			std::fprintf(stderr,
						 "Frame: seq=%u size=%u status=%s\n",
						 stats_.last_seq,
						 len,
						 valid ? "valid" : "invalid");
		}

		/*
		 * Send only valid frames to the viewer.
		 * TcpServer::send_frame already sends the 4-byte network-order length
		 * and then the frame data.
		 */
		if (valid && tcp_server_ && tcp_server_->has_client())
			tcp_server_->send_frame(buffer, len);

		return 0;
	}

	/*
	 * TODO: Validate a frame
	 *
	 * Check these things:
	 * 1. Magic number: first 4 bytes should be PHANTOMFPGA_FRAME_MAGIC
	 *    (read it as a struct phantomfpga_frame_header*)
	 *    Increment stats_.magic_errors on failure.
	 *
	 * 2. Sequence continuity: the sequence number should be
	 *    (stats_.last_seq + 1) % PHANTOMFPGA_FRAME_COUNT
	 *    Increment stats_.seq_errors on mismatch.
	 *    (Skip this check if !stats_.seq_initialized)
	 *
	 * 3. CRC32 (if config_.validate_crc):
	 *    Compute CRC32::compute(frame, PHANTOMFPGA_FRAME_SIZE - 4)
	 *    Compare with the stored CRC at offset (PHANTOMFPGA_FRAME_SIZE - 4)
	 *    Increment stats_.crc_errors on mismatch.
	 *
	 * Update stats_.last_seq and stats_.seq_initialized.
	 * Returns true if the frame is valid.
	 */
	bool validate_frame(const void *frame, uint32_t frame_size) override
	{
		if (!frame || frame_size != PHANTOMFPGA_FRAME_SIZE)
		{
			return false;
		}

		const uint8_t *bytes = static_cast<const uint8_t *>(frame);

		uint32_t magic = read_le32(bytes + 0);
		uint32_t seq = read_le32(bytes + 4);

		bool valid = true;

		if (magic != PHANTOMFPGA_FRAME_MAGIC)
		{
			stats_.magic_errors++;
			valid = false;
		}

		if (seq >= PHANTOMFPGA_FRAME_COUNT)
		{
			stats_.seq_errors++;
			valid = false;
		}

		if (valid && stats_.seq_initialized)
		{
			uint32_t expected = (stats_.last_seq + 1) % PHANTOMFPGA_FRAME_COUNT;

			if (seq != expected)
			{
				stats_.seq_errors++;
				valid = false;
			}
		}

		if (config_.validate_crc)
		{
			uint32_t computed_crc = CRC32::compute(frame, PHANTOMFPGA_FRAME_SIZE - 4);
			uint32_t stored_crc = read_le32(bytes + PHANTOMFPGA_FRAME_SIZE - 4);

			if (computed_crc != stored_crc)
			{
				stats_.crc_errors++;
				valid = false;
			}
		}

		if (valid)
		{
			stats_.last_seq = seq;
			stats_.seq_initialized = true;
		}

		return valid;
	}

	/*
	 * Print statistics
	 */
	void print_statistics() override
	{
		/* Print app-side stats */
		fprintf(stderr, "\n=== Application Statistics ===\n");
		fprintf(stderr, "Frames received: %lu\n", stats_.frames_received);
		fprintf(stderr, "Frames valid: %lu\n", stats_.frames_valid);
		fprintf(stderr, "Sequence errors: %lu\n", stats_.seq_errors);
		fprintf(stderr, "Magic errors: %lu\n", stats_.magic_errors);
		fprintf(stderr, "CRC errors: %lu\n", stats_.crc_errors);

		/* If tcp_server_: print network stats */
		if (tcp_server_)
		{
			fprintf(stderr, "\n=== Network Statistics ===\n");
			fprintf(stderr, "Frames sent: %lu\n", tcp_server_->stats().frames_sent);
			fprintf(stderr, "Bytes sent: %lu\n", tcp_server_->stats().bytes_sent);
		}

		/* Get device stats */
		if (dev_fd_.valid())
		{
			struct phantomfpga_stats dev_stats = {};
			int ret = ioctl(dev_fd_.get(), PHANTOMFPGA_IOCTL_GET_STATS, &dev_stats);
			if (!ret)
			{
				fprintf(stderr, "\n=== Device Statistics ===\n");
				fprintf(stderr, "Frames produced: %llu\n", dev_stats.frames_produced);
				fprintf(stderr, "Frames dropped: %llu\n", dev_stats.frames_dropped);
				fprintf(stderr, "Current frame: %u\n", dev_stats.current_frame);
			}
		}

		/* Calculate and print runtime duration */
		fprintf(stderr, "\n=== Runtime ===\n");
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		uint64_t elapsed_sec = now.tv_sec - stats_.start_time.tv_sec;
		int64_t elapsed_nsec = now.tv_nsec - stats_.start_time.tv_nsec;
		if (elapsed_nsec < 0)
		{
			elapsed_sec--;
			elapsed_nsec += 1000000000;
		}
		fprintf(stderr, "Runtime: %lu.%03lu seconds\n", elapsed_sec, elapsed_nsec / 1000000);
	}
};

/* ----------------------------------------------------------------------- */
/* main()                                                                  */
/* ----------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	PhantomFpgaAppImpl app;
	return app.run(argc, argv);
}
