#include "dm-fdekey.h"

static uuid_t pta_fed_key_uuid = UUID_INIT(0x89b589e9, 0x5339, 0x41d1,
					  0xa1, 0x38, 0xdc, 0xa3,
					  0x32, 0xcd, 0x0c, 0x88);

/*
* inout      params[0].memref = buffer
*/
#define PTA_GET_FDE_KEY 0

struct optee_ctx {
	struct tee_context *ctx; /**< Optee context */
	struct tee_ioctl_open_session_arg sess_arg; /**< Optee session */
};

static int optee_ctx_match(struct tee_ioctl_version_data *ver, const void *data)
{
	int ret = 0;

	if (ver->impl_id == TEE_IMPL_ID_OPTEE)
		ret = 1;

	return ret;
}

static int prepare_tee_session(struct optee_ctx *ctx)
{
	int32_t ret;

	memset(&ctx->sess_arg, 0, sizeof(ctx->sess_arg));

	ctx->ctx = tee_client_open_context(NULL, optee_ctx_match, NULL,
						NULL);
	if (IS_ERR(ctx->ctx)){
		pr_err("context create failed\n");
		return -ENODEV;
	}

	memcpy(ctx->sess_arg.uuid, &pta_fed_key_uuid, TEE_IOCTL_UUID_LEN);
	ctx->sess_arg.clnt_login = TEE_IOCTL_LOGIN_REE_KERNEL;
	ctx->sess_arg.num_params = 0;

	ret = tee_client_open_session(ctx->ctx, &ctx->sess_arg, NULL);
	if ((ret < 0) || (ctx->sess_arg.ret != 0)) {
		pr_err("tee_client_open_session failed, err: %x\n",
			ctx->sess_arg.ret);
		ret = -EINVAL;
	} else {
		goto exit_out_ctx;
	}

	tee_client_close_context(ctx->ctx);
exit_out_ctx:
	return ret;
}

static void terminate_tee_session(struct optee_ctx *ctx)
{
	tee_client_close_session(ctx->ctx, ctx->sess_arg.session);
	tee_client_close_context(ctx->ctx);
}

static unsigned char hex_char_to_bin(char c)
{
	char res = 0xFF;

	if (c >= '0' && c <= '9')
		res = c - '0';
	else if (c >= 'A' && c <= 'F')
		res = c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		res = c - 'a' + 10;
	else {
		pr_err("Invalid hex character: %c\n", c);
		res = 0xFF; // Invalid value
	}
	return res;
}

// Function to convert a hex string to binary data
static int hex_str_to_bin(const char *hex_str, unsigned char *bin_data, size_t bin_len)
{
	size_t hex_len = strlen(hex_str);
	size_t i;

	if (hex_len % 2 != 0) {
		pr_err("Invalid hex string length\n");
		return -EINVAL;
	}

	for (i = 0; i < hex_len; i += 2) {
		unsigned char high = hex_char_to_bin(hex_str[i]);
		unsigned char low = hex_char_to_bin(hex_str[i + 1]);

		if (high == 0xFF || low == 0xFF) {
			pr_err("Invalid hex character in string\n");
			return -EINVAL;
		}

		bin_data[i / 2] = (high << 4) | low;
	}

	return 0;
}

static int handle_key(unsigned char *key, char *dr_key)
{
	char *tmp;
	int key_str_len;
	int ret;

	tmp = strstr(key, ":");
	if (! (tmp + 1)) {
		return -1;
	}
	tmp += 1;
	key_str_len = strlen(tmp);

	ret = hex_str_to_bin(tmp, dr_key, key_str_len / 2);
	if (! ret) {
		ret = key_str_len >> 1;
	}

	return ret;
}

int get_fde_key_from_efuse(unsigned char *key, char *dr_key)
{
	int ret, key_len, i;
	struct optee_ctx ctx = {0};
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[4];
	struct tee_shm *reg_shm = NULL;
	char mid_key[512] = {0};

	key_len = handle_key(key, mid_key);
	if (key_len < 0) {
		ret = key_len;
		goto exit;
	}

	memset(&inv_arg, 0, sizeof(inv_arg));
	memset(&param, 0, sizeof(param));

	ret = prepare_tee_session(&ctx);
	if (ret != 0) {
		pr_err("prepare_tee_session failed with code 0x%x\n", ret);
		goto exit;
	}

	reg_shm = tee_shm_register_kernel_buf(ctx.ctx, mid_key, key_len);
	if (IS_ERR(reg_shm)) {
		pr_err("key shm register failed\n");
		goto exit;
	}

	inv_arg.func = PTA_GET_FDE_KEY;
	inv_arg.session = ctx.sess_arg.session;
	inv_arg.num_params = 1;

	memset(param, 0, sizeof(param));

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[0].u.memref.shm = reg_shm;
	param[0].u.memref.size = key_len;

	ret = tee_client_invoke_func(ctx.ctx, &inv_arg, param);
	if ((ret < 0) || (inv_arg.ret != 0)) {
		pr_err("PTA_GET_FDE_KEY invoke err: %x\n",
			inv_arg.ret);
		ret = -EFAULT;
	} else {
		ret = param[0].u.memref.size;
	}
	for (i = 0; i < key_len; i++) {
		sprintf(dr_key + i * 2, "%02x", mid_key[i]);
	}
	tee_shm_free(reg_shm);
	terminate_tee_session(&ctx);

exit:
	return ret;
}
