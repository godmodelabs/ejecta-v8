#ifndef __GLCOMPAT_H
#define __GLCOMPAT_H	1

#define OPENGL_ES	1


#ifdef OPENGL_ES

	#include <GLES/gl.h>
	#define GL_GLEXT_PROTOTYPES 1
	#include <GLES/glext.h>

	#define COMPAT_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
	#define COMPAT_GL_RENDERBUFFER GL_RENDERBUFFER_OES
	#define COMPAT_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
	#define COMPAT_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_OES
	#define COMPAT_glBindFramebuffer glBindFramebufferOES
	#define COMPAT_glGenFramebuffers glGenFramebuffersOES
#define COMPAT_glDeleteFramebuffers glDeleteFramebuffersOES

#define COMPAT_glBindRenderbuffer glBindRenderbufferOES
#define COMPAT_glGenRenderbuffers glGenRenderbuffersOES
#define COMPAT_glDeleteRenderbuffers glDeleteRenderbuffersOES
#define COMPAT_glFramebufferRenderbuffer glFramebufferRenderbufferOES
#else
	#define COMPAT_GL_FRAMEBUFFER GL_FRAMEBUFFER
	#define COMPAT_GL_RENDERBUFFER GL_RENDERBUFFER
	#define COMPAT_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
	#define COMPAT_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
	#define COMPAT_glBindFramebuffer glBindFramebuffer
	#define COMPAT_glGenFramebuffers glGenFramebuffers

#define COMPAT_glBindRenderbuffer glBindRenderbufferOES
#define COMPAT_glGenRenderbuffers glGenRenderbuffers
#define COMPAT_glFramebufferRenderbuffer glFramebufferRenderbuffer
#endif


#endif
