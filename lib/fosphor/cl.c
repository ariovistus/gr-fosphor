/*
 * cl.c
 *
 * OpenCL base routines
 *
 * Copyright (C) 2013 Sylvain Munaut
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \addtogroup cl
 *  @{
 */

/*! \file cl.c
 *  \brief OpenCL base routines
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cl_platform.h"

#if defined(__APPLE__) || defined(MAXOSX)
# include <OpenGL/OpenGL.h>
# include <OpenGL/gl.h>
#elif defined(_WIN32)
# include <Windows.h>
# include <Wingdi.h>
#else
# include <GL/glx.h>
#endif

#include "cl.h"
#include "gl.h"
#include "private.h"
#include "resource.h"


struct fosphor_cl_features
{
#define FLG_CL_GL_SHARING	(1<<0)
#define FLG_CL_NVIDIA_SM11	(1<<1)
#define FLG_CL_OPENCL_11	(1<<2)
#define FLG_CL_LOCAL_ATOMIC_EXT	(1<<3)

	cl_device_type type;
	char vendor[64];
	int local_mem;
	int flags;
	int wg_size;
	int wg_size_dim[2];
};

struct fosphor_cl_state
{
	cl_platform_id   pl_id;
	cl_device_id     dev_id;
	cl_context       ctx;
	cl_command_queue cq;

	/* Features */
	struct fosphor_cl_features feat;

	/* FFT */
	cl_mem		mem_fft_in;
	cl_mem		mem_fft_out;
	cl_mem		mem_fft_win;

	cl_program	prog_fft;
	cl_kernel	kern_fft;

	float		*fft_win;
	int		fft_win_updated;

	/* Display */
	cl_mem		mem_waterfall;
	cl_mem		mem_histogram;
	cl_mem		mem_spectrum;

	cl_program	prog_display;
	cl_kernel	kern_display;

	/* Histogram range */
	float		histo_scale;
	float		histo_offset;

	/* Waterfall position */
	int		waterfall_pos;
};

/* -------------------------------------------------------------------------- */
/* Helpers / Internal API                                                     */
/* -------------------------------------------------------------------------- */

#define MAX_PLATFORMS	16
#define MAX_DEVICES	16

#define CL_ERR_CHECK(v, msg)						\
	if ((v) != CL_SUCCESS) {					\
		fprintf(stderr, "[!] CL Error (%d, %s:%d): %s\n",	\
		        (v), __FILE__, __LINE__, msg);			\
		goto error;						\
	}


static int
cl_device_query(cl_device_id dev_id, struct fosphor_cl_features *feat)
{
	cl_ulong lmem;
	char txt[2048];
	cl_int err;
	int has_nv_attr;

	memset(feat, 0x00, sizeof(struct fosphor_cl_features));

	/* Device type */
	err = clGetDeviceInfo(dev_id, CL_DEVICE_TYPE, sizeof(cl_device_type), &feat->type, NULL);
	if (err != CL_SUCCESS)
		return -1;

	/* Vendor */
	err = clGetDeviceInfo(dev_id, CL_DEVICE_VENDOR, sizeof(feat->vendor)-1, &feat->vendor, NULL);
	if (err != CL_SUCCESS)
		return -1;

	/* Local memory size */
	err = clGetDeviceInfo(dev_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &lmem, NULL);
	if (err != CL_SUCCESS)
		return -1;

	feat->local_mem = lmem;

	/* CL/GL extension */
	err = clGetDeviceInfo(dev_id, CL_DEVICE_EXTENSIONS, sizeof(txt)-1, txt, NULL);
	if (err != CL_SUCCESS)
		return -1;

	txt[sizeof(txt)-1] = 0;

	/*  Check for CL/GL sharing */
	if (strstr(txt, "cl_khr_gl_sharing") || strstr(txt, "cl_APPLE_gl_sharing"))
		feat->flags |= FLG_CL_GL_SHARING;

	/* Check for NV attributes */
	has_nv_attr = !!strstr(txt, "cl_nv_device_attribute_query");

	/* Check for cl_khr_local_int32_base_atomics extension */
	if (strstr(txt, "cl_khr_local_int32_base_atomics"))
		feat->flags |= FLG_CL_LOCAL_ATOMIC_EXT;

	/* Check OpenCL 1.1 compat */
	err = clGetDeviceInfo(dev_id, CL_DEVICE_VERSION, sizeof(txt)-1, txt, NULL);
	if (err != CL_SUCCESS)
		return -1;

	txt[sizeof(txt)-1] = 0;

	if (!memcmp(txt, "OpenCL 1.", 9) && txt[9] >= '1')
		feat->flags |= FLG_CL_OPENCL_11;

	/* Check if a NVidia SM11 architecture */
	if (has_nv_attr) {
		cl_uint nv_maj, nv_min;

		err = clGetDeviceInfo(dev_id, CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,
		                      sizeof(cl_uint), &nv_maj, NULL);
		if (err != CL_SUCCESS)
			return -1;

		err = clGetDeviceInfo(dev_id, CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,
		                      sizeof(cl_uint), &nv_min, NULL);
		if (err != CL_SUCCESS)
			return -1;

		if ((nv_maj == 1) && (nv_min == 1))
			feat->flags |= FLG_CL_NVIDIA_SM11;
	}
#ifdef __APPLE__
	else if (!(feat->flags & (FLG_CL_OPENCL_11 | FLG_CL_LOCAL_ATOMIC_EXT)))
	{
		/*
		 * OSX doesn't allow query of NV attributes even on NVidia
		 * cards so we just assume any non-opencl 1.1 nvidia card
		 * without cl_khr_local_int32_base_atomics extension
		 * that does OpenCL is a SM1.1 one
		 */
		err = clGetDeviceInfo(dev_id, CL_DEVICE_VENDOR, sizeof(txt)-1, txt, NULL);
		if (err != CL_SUCCESS)
			return -1;

		txt[sizeof(txt)-1] = 0;

		if (!!strstr(txt, "NVIDIA"))
			feat->flags |= FLG_CL_NVIDIA_SM11;
	}
#endif

	return 0;
}

static int
cl_device_score(cl_device_id dev_id, struct fosphor_cl_features *feat)
{
	int rv, score = 0;

	/* Query device */
	rv = cl_device_query(dev_id, feat);
	if (rv)
		return rv;

	/* Check compatibility */
	if (!(feat->flags & FLG_CL_GL_SHARING))
		return -1;

	if (!(feat->flags & (FLG_CL_NVIDIA_SM11 | FLG_CL_OPENCL_11 | FLG_CL_LOCAL_ATOMIC_EXT)))
		return -1;

	/* Prefer GPU (preferrably NVidia / AMD) */
	if (feat->type == CL_DEVICE_TYPE_GPU)
	{
		char vendor[sizeof(feat->vendor)];
		int i;

		score += 1000;

		for (i=0; i<sizeof(feat->vendor); i++)
			vendor[i] = tolower(feat->vendor[i]);

		if (strstr(vendor, "nvidia") ||
		    strstr(vendor, "advanced micro devices") ||
		    strstr(vendor, "amd"))
			score += 500;
	}

	/* Bigger local mem */
	score += (feat->local_mem >> 10);

	return score;
}

static int
cl_find_device(cl_platform_id *pl_id_p, cl_device_id *dev_id_p,
               struct fosphor_cl_features *feat)
{
	cl_platform_id pl_list[MAX_PLATFORMS], pl_id;
	cl_device_id dev_list[MAX_DEVICES], dev_id;
	cl_uint pl_count, dev_count, i, j;
	cl_int err;
	int score = -1;

	/* Scan each platforms */
	err = clGetPlatformIDs(MAX_PLATFORMS, pl_list, &pl_count);
	CL_ERR_CHECK(err, "Unable to fetch platform IDs");

	for (i=0; i<pl_count; i++)
	{
		/* Scan all devices */
		err = clGetDeviceIDs(pl_list[i], CL_DEVICE_TYPE_ALL, MAX_DEVICES, dev_list, &dev_count);
		CL_ERR_CHECK(err, "Unable to fetch device IDs");

		for (j=0; j<dev_count; j++)
		{
			struct fosphor_cl_features feat_cur;
			int s = cl_device_score(dev_list[j], &feat_cur);
			if (s > score) {
				pl_id  = pl_list[i];
				dev_id = dev_list[j];
				memcpy(feat, &feat_cur, sizeof(struct fosphor_cl_features));
				score = s;
			}
		}
	}

	/* Did we get a good fit ? */
	if (score >= 0) {
		*pl_id_p  = pl_id;
		*dev_id_p = dev_id;
		err = 0;
	} else {
		err = -ENODEV;
	}

error:
	return err;
}

static cl_program
cl_load_program(cl_device_id dev_id, cl_context ctx,
                const char *resource_name, const char *opts)
{
	cl_program prog = NULL;
	const char *src;
	cl_int err;

	/* Grab resource */
	src = resource_get(resource_name, NULL);
	if (!src) {
		fprintf(stderr, "[!] Unable to load non-existent resource '%s'\n", resource_name);
		goto error;
	}

	/* Create the program from sources */
	prog = clCreateProgramWithSource(ctx, 1, (const char **)&src, NULL, &err);
	CL_ERR_CHECK(err, "Failed to create program");

        /* Build it */
        err = clBuildProgram(prog, 0, NULL, opts, NULL, NULL);

#ifndef DEBUG_CL
        if (err != CL_SUCCESS)
#endif
        {
		size_t len;
		const int txt_buf_len = 1024 * 1024;
		char *txt_buf;

		txt_buf = malloc(txt_buf_len);
                clGetProgramBuildInfo(prog, dev_id, CL_PROGRAM_BUILD_LOG, txt_buf_len, txt_buf, &len);
                fprintf(stderr, "Build log for '%s':\n%s\n\n---\n", resource_name, txt_buf);
		free(txt_buf);
        }

	CL_ERR_CHECK(err, "Failed to build program");

#ifdef DEBUG_CL
	{
		size_t bin_len;
		char name_buf[256];
		char *bin_buf;

		snprintf(name_buf, 256, "prog_%s.bin", resource_name);

		clGetProgramInfo(prog, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &bin_len, NULL);
		fprintf(stderr, "Binary length for '%s': %d\n\n", resource_name, (int)bin_len);

		bin_buf = malloc(bin_len);

		clGetProgramInfo(prog, CL_PROGRAM_BINARIES, bin_len, &bin_buf, NULL );

		FILE *fh = fopen(name_buf, "wb");
		if (fwrite(bin_buf, bin_len, 1, fh) != 1)
			fprintf(stderr, "[w] Binary write failed\n");
		fclose(fh);

		free(bin_buf);
	}
#endif

	/* All good */
	return prog;

	/* Error path */
error:
	if (prog)
		clReleaseProgram(prog);

	return NULL;
}

static int
cl_do_init(struct fosphor *self)
{
	struct fosphor_cl_state *cl = self->cl;
	cl_context_properties ctx_props[7];
	const char *disp_opts;
	cl_int err;

	/* Context */
#if defined(__APPLE__) || defined(MAXOSX)

		/* OSX variant */
	ctx_props[0] = CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE;
	ctx_props[1] = (cl_context_properties) CGLGetShareGroup(CGLGetCurrentContext());
	ctx_props[2] = 0;

#elif defined(__WIN32__) || defined(_WIN32)

		/* Win 32 variant */
	ctx_props[0] = CL_GL_CONTEXT_KHR;
	ctx_props[1] = (cl_context_properties) wglGetCurrentContext();
	ctx_props[2] = CL_WGL_HDC_KHR;
	ctx_props[3] = (cl_context_properties) wglGetCurrentDC();
	ctx_props[4] = CL_CONTEXT_PLATFORM;
	ctx_props[5] = (cl_context_properties) cl->pl_id;
	ctx_props[6] = 0;

#else

		/* Linux variant */
	ctx_props[0] = CL_GL_CONTEXT_KHR;
	ctx_props[1] = (cl_context_properties) glXGetCurrentContext();
	ctx_props[2] = CL_GLX_DISPLAY_KHR;
	ctx_props[3] = (cl_context_properties) glXGetCurrentDisplay();
	ctx_props[4] = CL_CONTEXT_PLATFORM;
	ctx_props[5] = (cl_context_properties) cl->pl_id;
	ctx_props[6] = 0;

#endif

	cl->ctx = clCreateContext(ctx_props, 1, &cl->dev_id, NULL, NULL, &err);
	CL_ERR_CHECK(err, "Unable to create context");

	/* Command Queue */
	cl->cq = clCreateCommandQueue(cl->ctx, cl->dev_id, 0, &err);
	CL_ERR_CHECK(err, "Unable to create command queue");

	/* FFT buffers */
	cl->mem_fft_in = clCreateBuffer(cl->ctx,
		CL_MEM_READ_ONLY,
		2 * sizeof(cl_float) * FOSPHOR_FFT_LEN * FOSPHOR_FFT_MAX_BATCH,
		NULL,
		&err
	);
	CL_ERR_CHECK(err, "Unable to allocate FFT input buffer");

	cl->mem_fft_out = clCreateBuffer(cl->ctx,
		CL_MEM_READ_WRITE,
		2 * sizeof(cl_float) * FOSPHOR_FFT_LEN * FOSPHOR_FFT_MAX_BATCH,
		NULL,
		&err
	);
	CL_ERR_CHECK(err, "Unable to allocate FFT output buffer");

	cl->mem_fft_win = clCreateBuffer(cl->ctx,
		CL_MEM_READ_ONLY,
		2 * sizeof(cl_float) * FOSPHOR_FFT_LEN,
		NULL,
		&err
	);
	CL_ERR_CHECK(err, "Unable to allocate FFT window buffer");

	/* FFT program/kernels */
	cl->prog_fft = cl_load_program(cl->dev_id, cl->ctx, "fft.cl", NULL);
	if (!cl->prog_fft)
		goto error;

	cl->kern_fft = clCreateKernel(cl->prog_fft, "fft1D_1024", &err);
	CL_ERR_CHECK(err, "Unable to create FFT kernel");

	/* Configure static FFT kernel args */
	err  = clSetKernelArg(cl->kern_fft, 0, sizeof(cl_mem), &cl->mem_fft_in);
	err |= clSetKernelArg(cl->kern_fft, 1, sizeof(cl_mem), &cl->mem_fft_out);
	err |= clSetKernelArg(cl->kern_fft, 2, sizeof(cl_mem), &cl->mem_fft_win);

	CL_ERR_CHECK(err, "Unable to configure FFT kernel");

	/* GL shared objects */
		/* Waterfall texture */
	cl->mem_waterfall = clCreateFromGLTexture2D(cl->ctx,
		CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0,
		fosphor_gl_get_shared_id(self, GL_ID_TEX_WATERFALL),
		&err
	);
	CL_ERR_CHECK(err, "Unable to share waterfall texture into OpenCL context");

		/* Histogram texture */
	cl->mem_histogram = clCreateFromGLTexture2D(cl->ctx,
		CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0,
		fosphor_gl_get_shared_id(self, GL_ID_TEX_HISTOGRAM),
		&err
	);
	CL_ERR_CHECK(err, "Unable to share histogram texture into OpenCL context");

		/* Spectrum VBO */
	cl->mem_spectrum = clCreateFromGLBuffer(cl->ctx,
		CL_MEM_WRITE_ONLY,
		fosphor_gl_get_shared_id(self, GL_ID_VBO_SPECTRUM),
		&err
	);
	CL_ERR_CHECK(err, "Unable to share spectrum VBO into OpenCL context");

	/* Display program/kernel */
	if (cl->feat.flags & FLG_CL_NVIDIA_SM11)
		disp_opts = "-DUSE_NV_SM11_ATOMICS";
	else if (!(cl->feat.flags & FLG_CL_OPENCL_11))
		disp_opts = "-DUSE_EXT_ATOMICS";
	else
		disp_opts = NULL;

	cl->prog_display = cl_load_program(cl->dev_id, cl->ctx, "display.cl", disp_opts);
	if (!cl->prog_display)
		goto error;

	cl->kern_display = clCreateKernel(cl->prog_display, "display", &err);
	CL_ERR_CHECK(err, "Unable to create display kernel");

	/* Configure static display kernel args */
	{
	cl_uint fft_log2_len = FOSPHOR_FFT_LEN_LOG;
	cl_float histo_t0r   = 16.0f;
	cl_float histo_t0d   = 1024.0f;
	cl_float live_alpha  = 0.002f;

	err  = clSetKernelArg(cl->kern_display,  0, sizeof(cl_mem),   &cl->mem_fft_out);
	err |= clSetKernelArg(cl->kern_display,  1, sizeof(cl_int),   &fft_log2_len);

	err |= clSetKernelArg(cl->kern_display,  3, sizeof(cl_mem),   &cl->mem_waterfall);

	err |= clSetKernelArg(cl->kern_display,  5, sizeof(cl_mem),   &cl->mem_histogram);
	err |= clSetKernelArg(cl->kern_display,  6, sizeof(cl_mem),   &cl->mem_histogram);
	err |= clSetKernelArg(cl->kern_display,  7, sizeof(cl_float), &histo_t0r);
	err |= clSetKernelArg(cl->kern_display,  8, sizeof(cl_float), &histo_t0d);

	err |= clSetKernelArg(cl->kern_display, 11, sizeof(cl_mem),   &cl->mem_spectrum);
	err |= clSetKernelArg(cl->kern_display, 12, sizeof(cl_float), &live_alpha);

	CL_ERR_CHECK(err, "Unable to configure display kernel");

	/* All done */
	err = 0;
	}

error:
	return err;
}

static void
cl_do_release(struct fosphor_cl_state *cl)
{
	if (cl->kern_display)
		clReleaseKernel(cl->kern_display);

	if (cl->prog_display)
		clReleaseProgram(cl->prog_display);

	if (cl->mem_spectrum)
		clReleaseMemObject(cl->mem_spectrum);

	if (cl->mem_histogram)
		clReleaseMemObject(cl->mem_histogram);

	if (cl->mem_waterfall)
		clReleaseMemObject(cl->mem_waterfall);

	if (cl->kern_fft)
		clReleaseKernel(cl->kern_fft);

	if (cl->prog_fft)
		clReleaseProgram(cl->prog_fft);

	if (cl->mem_fft_out)
		clReleaseMemObject(cl->mem_fft_out);

	if (cl->mem_fft_in)
		clReleaseMemObject(cl->mem_fft_in);

	if (cl->cq)
		clReleaseCommandQueue(cl->cq);

	if (cl->ctx)
		clReleaseContext(cl->ctx);
}


/* -------------------------------------------------------------------------- */
/* Exposed API                                                                */
/* -------------------------------------------------------------------------- */

int
fosphor_cl_init(struct fosphor *self)
{
	struct fosphor_cl_state *cl;
	char dev_name[128];
	cl_int err;

	/* Allocate structure */
	cl = malloc(sizeof(struct fosphor_cl_state));
	if (!cl)
		return -ENOMEM;

	self->cl = cl;

	memset(cl, 0, sizeof(struct fosphor_cl_state));

	/* Find suitable device */
	if (cl_find_device(&cl->pl_id, &cl->dev_id, &cl->feat)) {
		fprintf(stderr, "[!] No suitable OpenCL device found\n");
		goto error;
	}

	/* Report selected device */
	err = clGetDeviceInfo(cl->dev_id, CL_DEVICE_NAME, sizeof(dev_name)-1, dev_name, NULL);
	CL_ERR_CHECK(err, "Unable to fetch device name");

	fprintf(stderr, "[+] Selected device: %s\n", dev_name);

	/* Initialize selected platform / device */
	err = cl_do_init(self);
	if (err)
		goto error;

	/* Done */
	return 0;

	/* Error path */
error:
	fosphor_cl_release(self);

	return -EIO;
}

void
fosphor_cl_release(struct fosphor *self)
{
	struct fosphor_cl_state *cl = self->cl;

	/* Safety */
	if (!cl)
		return;

	/* Release all allocated OpenCL resources */
	cl_do_release(cl);

	/* Release structure */
	free(cl);

	/* Nothing left */
	self->cl = NULL;
}

int
fosphor_cl_process(struct fosphor *self,
                   void *samples, int len)
{
	struct fosphor_cl_state *cl = self->cl;

	cl_int err;
	size_t local[2], global[2];
	int n_spectra = len / FOSPHOR_FFT_LEN;
	cl_mem objs[3];

	/* Validate batch size */
	if (len & ((FOSPHOR_FFT_MULT_BATCH*FOSPHOR_FFT_LEN)-1))
		return -EINVAL;

	if (len > (FOSPHOR_FFT_LEN * FOSPHOR_FFT_MAX_BATCH))
		return -EINVAL;

	/* Copy new window if needed */
	if (cl->fft_win_updated) {
		err = clEnqueueWriteBuffer(
			cl->cq,
			cl->mem_fft_win,
			CL_FALSE,
			0, sizeof(cl_float) * FOSPHOR_FFT_LEN, cl->fft_win,
			0, NULL, NULL
		);
		CL_ERR_CHECK(err, "Unable to copy data to FFT window buffer");

		cl->fft_win_updated = 0;
	}

	/* Copy samples data */
	err = clEnqueueWriteBuffer(
		cl->cq,
		cl->mem_fft_in,
		CL_FALSE,
		0, 2 * sizeof(cl_float) * len, samples,
		0, NULL, NULL
	);
	CL_ERR_CHECK(err, "Unable to copy data to FFT input buffer");

	/* Execute FFT kernel */
	global[0] = FOSPHOR_FFT_LEN / 8;
	global[1] = n_spectra;

	local[0] = global[0];
	local[1] = 1;

	err = clEnqueueNDRangeKernel(cl->cq, cl->kern_fft, 2, NULL, global, local, 0, NULL, NULL);
	CL_ERR_CHECK(err, "Unable to queue FFT kernel execution");

	/* GL Objects */
	objs[0] = cl->mem_waterfall;
	objs[2] = cl->mem_histogram;
	objs[1] = cl->mem_spectrum;

	/* Capture all GL objects */
	err = clEnqueueAcquireGLObjects(cl->cq, 3, objs, 0, NULL, NULL);
	CL_ERR_CHECK(err, "Unable to acquire GL objects");

	/* Configure display kernel */
	err  = 0;
	err |= clSetKernelArg(cl->kern_display,  2, sizeof(cl_int),   &n_spectra);
	err |= clSetKernelArg(cl->kern_display,  4, sizeof(cl_int),   &cl->waterfall_pos);
	err |= clSetKernelArg(cl->kern_display,  9, sizeof(cl_float), &cl->histo_scale);
	err |= clSetKernelArg(cl->kern_display, 10, sizeof(cl_float), &cl->histo_offset);
	CL_ERR_CHECK(err, "Unable to configure display kernel");

	/* Execute display kernel */
	global[0] = FOSPHOR_FFT_LEN;
	global[1] = 16;
	local[0] = 16;
	local[1] = 16;

	err = clEnqueueNDRangeKernel(cl->cq, cl->kern_display, 2, NULL, global, local, 0, NULL, NULL);
	CL_ERR_CHECK(err, "Unable to queue display kernel execution");

	/* Release all GL objects */
	err = clEnqueueReleaseGLObjects(cl->cq, 3, objs, 0, NULL, NULL);
	CL_ERR_CHECK(err, "Unable to release GL objects");

	/* Ensure CL is done */
	clFinish(cl->cq);

	/* Advance waterfall */
	cl->waterfall_pos = (cl->waterfall_pos + n_spectra) & 1023;

	return 0;

error:
	return -EIO;
}


void
fosphor_cl_load_fft_window(struct fosphor *self, float *win)
{
	struct fosphor_cl_state *cl = self->cl;

	cl->fft_win = win;
	cl->fft_win_updated = 1;
}

int
fosphor_cl_get_waterfall_position(struct fosphor *self)
{
	struct fosphor_cl_state *cl = self->cl;

	return cl->waterfall_pos;
}

void
fosphor_cl_set_histogram_range(struct fosphor *self,
                               float scale, float offset)
{
	struct fosphor_cl_state *cl = self->cl;

	cl->histo_scale  = scale * 128.0f;
	cl->histo_offset = offset;
}

/*! @} */
