/* ======== SourceHook ========
* Copyright (C) 2004-2005 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/

/**
*	@file sourcehook.h
*	@brief Contains the public SourceHook API
*/

#ifndef __SOURCEHOOK_H__
#define __SOURCEHOOK_H__

// Interface revisions:
//  1 - Initial revision
//  2 - Changed to virtual functions for iterators and all queries
//  3 - Added "hook loop status variable"
//  4 - Reentrant
#define SH_IFACE_VERSION 4
#define SH_IMPL_VERSION 3

// Hookman version:
//  1 - Support for recalls, performance optimisations
#define SH_HOOKMAN_VERSION 1

// The value of SH_GLOB_SHPTR has to be a pointer to SourceHook::ISourceHook
// It's used in various macros
#ifndef SH_GLOB_SHPTR
#define SH_GLOB_SHPTR g_SHPtr
#endif

// Used to identify the current plugin
#ifndef SH_GLOB_PLUGPTR
#define SH_GLOB_PLUGPTR g_PLID
#endif

#ifdef SH_DEBUG
# include <stdio.h>
# include <stdlib.h>

# define SH_ASSERT(x, info) \
	do { \
		if (!(x)) \
		{ \
			printf("SOURCEHOOK DEBUG ASSERTION FAILED: %s:%u(%s): %s: ", __FILE__, __LINE__, __FUNCTION__, #x); \
			printf info; \
			putchar('\n'); \
			abort(); \
		} \
	} while(0)

#else
# define SH_ASSERT(x, info)
#endif

// System
#define SH_SYS_WIN32 1
#define SH_SYS_LINUX 2

#ifdef _WIN32
# define SH_SYS SH_SYS_WIN32
#elif defined __linux__
# define SH_SYS SH_SYS_LINUX
#else
# error Unsupported system
#endif

// Compiler
#define SH_COMP_GCC 1
#define SH_COMP_MSVC 2

#ifdef _MSC_VER
# define SH_COMP SH_COMP_MSVC
#elif defined __GNUC__
# define SH_COMP SH_COMP_GCC
#else
# error Unsupported compiler
#endif

#if SH_COMP==SH_COMP_MSVC
# define vsnprintf _vsnprintf
#endif

#if SH_SYS != SH_SYS_WIN32
# include <unistd.h>
#endif

#define SH_PTRSIZE sizeof(void*)

#include "FastDelegate.h"
#include "sh_memfuncinfo.h"

// Good old metamod!

// Flags returned by a plugin's api function.
// NOTE: order is crucial, as greater/less comparisons are made.
enum META_RES
{
	MRES_IGNORED=0,		// plugin didn't take any action
	MRES_HANDLED,		// plugin did something, but real function should still be called
	MRES_OVERRIDE,		// call real function, but use my return value
	MRES_SUPERCEDE		// skip real function; use my return value
};


namespace SourceHook
{
	/**
	*	@brief	Specifies the size (in bytes) for the internal buffer of vafmt(printf-like) function handlers
	*/
	const int STRBUF_LEN=4096;

	/**
	*	@brief	An empty class. No inheritance used. Used for original-function-call hacks
	*/
	class EmptyClass
	{
	};

	/**
	*	@brief Implicit cast.
	*/
	template <class In, class Out>
		inline Out implicit_cast(In input)
		{
			return input;
		}

	/**
	*	@brief A plugin typedef
	*
	*	SourceHook doesn't really care what this is. As long as the ==, != and = operators work on it and every
	*	plugin has a unique identifier, everything is ok.
	*	It should also be possible to set it to 0.
	*/
	typedef int Plugin;

	struct ProtoInfo
	{
		ProtoInfo(int rtsz, int nop, const int *p) : beginningNull(0), retTypeSize(rtsz), numOfParams(nop), params(p)
		{
		}
		int beginningNull;		//!< To distinguish from old protos (which never begin with 0)
		int retTypeSize;		//!< 0 if void
		int numOfParams;		//!< number of parameters
		const int *params;		//!< params[0]=0 (or -1 for vararg), params[1]=size of first param, ...
	};

	/**
	*	@brief Specifies the actions for hook managers
	*/
	enum HookManagerAction
	{
		HA_GetInfo = 0,			//!< Store info about the hook manager
		HA_Register,			//!< Save the IHookManagerInfo pointer for future reference
		HA_Unregister			//!< Clear the saved pointer
	};

	struct IHookManagerInfo;

	/**
	*	@brief Pointer to hook manager interface function
	*
	*	A "hook manager" is a the only thing that knows the actual protoype of the function at compile time.
	*
	*   @param ha What the hook manager should do
	*	@param hi A pointer to IHookManagerInfo
	*/
	typedef int (*HookManagerPubFunc)(HookManagerAction ha, IHookManagerInfo *hi);

	class ISHDelegate
	{
	public:
		virtual void DeleteThis() = 0;				// Ugly, I know
		virtual bool IsEqual(ISHDelegate *other) = 0;
	};

	template <class T> class CSHDelegate : public ISHDelegate
	{
		T m_Deleg;
	public:
		CSHDelegate(T deleg) : m_Deleg(deleg)
		{
		}

		CSHDelegate(const CSHDelegate &other) : m_Deleg(other.m_Deleg)
		{
		}

		void DeleteThis()
		{
			delete this;
		}

		bool IsEqual(ISHDelegate *other)
		{
			return static_cast<CSHDelegate<T>* >(other)->GetDeleg() == GetDeleg();
		}

		T &GetDeleg()
		{
			return m_Deleg;
		}
	};

	struct IHookList
	{
		struct IIter
		{
			virtual bool End() = 0;
			virtual void Next() = 0;
			virtual ISHDelegate *Handler() = 0;
			virtual int ThisPtrOffs() = 0;
		};
		virtual IIter *GetIter() = 0;
		virtual void ReleaseIter(IIter *pIter) = 0;
	};

	struct IIface
	{
		virtual void *GetPtr() = 0;
		virtual IHookList *GetPreHooks() = 0;
		virtual IHookList *GetPostHooks() = 0;
	};

	struct IVfnPtr
	{
		virtual void *GetVfnPtr() = 0;
		virtual void *GetOrigEntry() = 0;

		virtual IIface *FindIface(void *ptr) = 0;
	};

	struct IHookManagerInfo
	{
		virtual IVfnPtr *FindVfnPtr(void *vfnptr) = 0;

		virtual void SetInfo(int vtbloffs, int vtblidx, const char *proto) = 0;
		virtual void SetHookfuncVfnptr(void *hookfunc_vfnptr) = 0;

		// Added 23.12.2005 (yup! I'm coding RIGHT BEFORE CHRISTMAS!)
		// If the hookman doesn't set this, it defaults 0
		// SourceHook prefers hookmans with higher version numbers
		virtual void SetVersion(int version) = 0;
	};

	class AutoHookIter
	{
		IHookList *m_pList;
		IHookList::IIter *m_pIter;
	public:
		AutoHookIter(IHookList *pList)
			: m_pList(pList), m_pIter(pList->GetIter())
		{
		}

		~AutoHookIter()
		{
			if (m_pList)
				m_pList->ReleaseIter(m_pIter);
		}

		bool End()
		{
			return m_pIter->End();
		}

		void Next()
		{
			m_pIter->Next();
		}

		ISHDelegate *Handler()
		{
			return m_pIter->Handler();
		}

		int ThisPtrOffs()
		{
			return m_pIter->ThisPtrOffs();
		}

		void SetToZero()
		{
			m_pList = 0;
		}
	};

	template<class B> struct CallClass
	{
		virtual B *GetThisPtr() = 0;
		virtual void *GetOrigFunc(int vtbloffs, int vtblidx) = 0;
	};

	typedef CallClass<void> GenericCallClass;

	/**
	*	@brief The main SourceHook interface
	*/
	class ISourceHook
	{
	public:
		/**
		*	@brief Return interface version
		*/
		virtual int GetIfaceVersion() = 0;

		/**
		*	@brief Return implementation version
		*/
		virtual int GetImplVersion() = 0;

		/**
		*	@brief Add a hook.
		*
		*	@return True if the function succeeded, false otherwise
		*
		*	@param plug The unique identifier of the plugin that calls this function
		*	@param iface The interface pointer
		*	@param ifacesize The size of the class iface points to
		*	@param myHookMan A hook manager function that should be capable of handling the function
		*	@param handler A pointer to a FastDelegate containing the hook handler
		*	@param post Set to true if you want a post handler
		*/
		virtual bool AddHook(Plugin plug, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
			ISHDelegate *handler, bool post) = 0;

		/**
		*	@brief Removes a hook.
		*
		*	@return True if the function succeeded, false otherwise
		*
		*	@param plug The unique identifier of the plugin that calls this function
		*	@param iface The interface pointer
		*	@param myHookMan A hook manager function that should be capable of handling the function
		*	@param handler A pointer to a FastDelegate containing the hook handler
		*	@param post Set to true if you want a post handler
		*/
		virtual bool RemoveHook(Plugin plug, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
			ISHDelegate *handler, bool post) = 0;

		/**
		*	@brief Checks whether a plugin has (a) hook manager(s) that is/are currently used by other plugins
		*
		*	@param plug The unique identifier of the plugin in question
		*/
		virtual bool IsPluginInUse(Plugin plug) = 0;

		/**
		*	@brief Return a pointer to a callclass. Generate a new one if required.
		*
		*	@param iface The interface pointer
		*	@param size Size of the class instance
		*/
		virtual GenericCallClass *GetCallClass(void *iface, size_t size) = 0;

		/**
		*	@brief Release a callclass
		*
		*	@param ptr Pointer to the callclass
		*/
		virtual void ReleaseCallClass(GenericCallClass *ptr) = 0;

		virtual void SetRes(META_RES res) = 0;				//!< Sets the meta result
		virtual META_RES GetPrevRes() = 0;					//!< Gets the meta result of the
															//!<  previously calledhandler
		virtual META_RES GetStatus() = 0;					//!< Gets the highest meta result
		virtual const void *GetOrigRet() = 0;				//!< Gets the original result.
															//!<  If not in post function, undefined
		virtual const void *GetOverrideRet() = 0;			//!< Gets the override result.
															//!<  If none is specified, NULL
		virtual void *GetIfacePtr() = 0;					//!< Gets the interface pointer
		//////////////////////////////////////////////////////////////////////////
		// For hook managers
		virtual void HookLoopBegin(IIface *pIface) = 0;			//!< Should be called when a hook loop begins
		virtual void HookLoopEnd() = 0;							//!< Should be called when a hook loop exits
		virtual void SetCurResPtr(META_RES *mres) = 0;			//!< Sets pointer to the current meta result
		virtual void SetPrevResPtr(META_RES *mres) = 0;			//!< Sets pointer to previous meta result
		virtual void SetStatusPtr(META_RES *mres) = 0;			//!< Sets pointer to the status variable
		virtual void SetIfacePtrPtr(void **pp) = 0;				//!< Sets pointer to the interface this pointer
		virtual void SetOrigRetPtr(const void *ptr) = 0;		//!< Sets the original return pointer
		virtual void SetOverrideRetPtr(void *ptr) = 0;			//!< Sets the override result pointer
		virtual bool ShouldContinue() = 0;						//!< Returns false if the hook loop should exit

		/**
		*	@brief Remove a hook manager. Auto-removes all hooks attached to it from plugin plug.
		*
		*	@param plug The owner of the hook manager
		*   @param pubFunc The hook manager's info function
		*/
		virtual void RemoveHookManager(Plugin plug, HookManagerPubFunc pubFunc) = 0;

		virtual void DoRecall() = 0;							//!< Initiates a recall sequence
		/*
			HOW RECALLS WORK:
			
			The problem:
				Users want the ability to change parameters of the called function
				from inside their handler.
			The solution:
				1) Mark as "recall"
				2) Recall the function
				3) => SH's hook func gets called:
				  4) The first iterator points at the first hook the last hookfunc didn't execute yet
				  5) does all iteration and returns normally
		        6) The user's handler returns immediately
				7) The hook func returns immediately as well

				Also note that the recalled hookfunc starts with the status the recalling hookfunc
				ended with. The last handler (doing the recall) is also able to specify its own
				META_RES.
		*/

		virtual void *GetOverrideRetPtr() = 0;		//!< Returns the pointer set by SetOverrideRetPtr

		/**
		*	@brief Set up the hook loop. Equivalent to calling:
		*	SetStatusPtr, SetPrevResPtr, SetCurResPtr, SetIfacePtrPtr, SetOrigRetPtr, Get/SetOverrideRetPtr
		*
		*	@param statusPtr		pointer to status variable
		*	@param prevResPtr		pointer to previous result variable
		*	@param curResPtr		pointer to current result variable
		*	@param ifacePtrPtr		pointer to interface this pointer variable
		*	@param origRetPr		pointer to original return value variable. NULL for void funcs
		*	@param overrideRetPtr	pointer to override return value variable. NULL for void funcs
		*	
		*	@return Override Return Pointer the hookfunc should use (may differ from overrideRetPtr
		*		when the hook func is being called as part of a recall
		*/
		virtual void *SetupHookLoop(META_RES *statusPtr, META_RES *prevResPtr, META_RES *curResPtr,
			void **ifacePtrPtr, const void *origRetPtr, void *overrideRetPtr) = 0;

		//!< 
	};
}

/************************************************************************/
/* High level interface                                                 */
/************************************************************************/
#define META_RESULT_STATUS					SH_GLOB_SHPTR->GetStatus()
#define META_RESULT_PREVIOUS				SH_GLOB_SHPTR->GetPrevRes()
#define META_RESULT_ORIG_RET(type)			*reinterpret_cast<const type*>(SH_GLOB_SHPTR->GetOrigRet())
#define META_RESULT_OVERRIDE_RET(type)		*reinterpret_cast<const type*>(SH_GLOB_SHPTR->GetOverrideRet())
#define META_IFACEPTR(type)					reinterpret_cast<type*>(SH_GLOB_SHPTR->GetIfacePtr())

#define SET_META_RESULT(result)				SH_GLOB_SHPTR->SetRes(result)
#define RETURN_META(result)					do { SET_META_RESULT(result); return; } while(0)
#define RETURN_META_VALUE(result, value)	do { SET_META_RESULT(result); return (value); } while(0)


// NEVER-EVER call these from post hooks!
// also, only call it from the hook handlers directly!
// :TODO: enforce it

// Why take a memfuncptr instead of iface and func when we have to deduce the iface anyway now?
// Well, without it, there'd be no way to specify which overloaded version we want in _VALUE

// SourceHook::SetOverrideRet is defined later.
#define RETURN_META_NEWPARAMS(result, memfuncptr, newparams) \
	do { \
		SET_META_RESULT(result); \
		SH_GLOB_SHPTR->DoRecall(); \
		(SourceHook::RecallGetIface(SH_GLOB_SHPTR, memfuncptr)->*(memfuncptr)) newparams; \
		RETURN_META(MRES_SUPERCEDE); \
	} while (0)

#define RETURN_META_VALUE_NEWPARAMS(result, value, memfuncptr, newparams) \
	do { \
		SET_META_RESULT(result); \
		SH_GLOB_SHPTR->DoRecall(); \
		if ((result) >= MRES_OVERRIDE) \
		{ \
			/* meh, set the override result here because we don't get a chance to return */ \
			/* before continuing the hook loop through the recall */ \
			SourceHook::SetOverrideResult(SH_GLOB_SHPTR, memfuncptr, value); \
		} \
		RETURN_META_VALUE(MRES_SUPERCEDE, \
			(SourceHook::RecallGetIface(SH_GLOB_SHPTR, memfuncptr)->*(memfuncptr)) newparams); \
	} while (0)

/**
*	@brief Get/generate callclass for an interface pointer
*
*	@param ifaceptr The interface pointer
*/
template<class ifacetype>
inline SourceHook::CallClass<ifacetype> *SH_GET_CALLCLASS_R(SourceHook::ISourceHook *shptr, ifacetype *ptr)
{
	return reinterpret_cast<SourceHook::CallClass<ifacetype>*>(
		shptr->GetCallClass(reinterpret_cast<void*>(ptr), sizeof(ifacetype)));
}

template<class ifacetype>
inline void SH_RELEASE_CALLCLASS_R(SourceHook::ISourceHook *shptr, SourceHook::CallClass<ifacetype> *ptr)
{
	shptr->ReleaseCallClass(reinterpret_cast<SourceHook::GenericCallClass*>(ptr));
}

#define SH_MANUALHOOK_RECONFIGURE(hookname, pvtblindex, pvtbloffs, pthisptroffs) \
	do { \
		SH_GLOB_SHPTR->RemoveHookManager(SH_GLOB_PLUGPTR, SH_MFHCls(hookname)::HookManPubFunc); \
		SH_MFHCls(hookname)::ms_MFI.thisptroffs = pthisptroffs; \
		SH_MFHCls(hookname)::ms_MFI.vtblindex = pvtblindex; \
		SH_MFHCls(hookname)::ms_MFI.vtbloffs = pvtbloffs; \
	} while (0)

#define SH_GET_CALLCLASS(ptr) SH_GET_CALLCLASS_R(SH_GLOB_SHPTR, ptr)
#define SH_RELEASE_CALLCLASS(ptr) SH_RELEASE_CALLCLASS_R(SH_GLOB_SHPTR, ptr)

#define SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, handler, post) \
	__SourceHook_FHAdd##ifacetype##ifacefunc((void*)SourceHook::implicit_cast<ifacetype*>(ifaceptr), \
	post, handler)
#define SH_ADD_HOOK_STATICFUNC(ifacetype, ifacefunc, ifaceptr, handler, post) \
	SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_ADD_HOOK_MEMFUNC(ifacetype, ifacefunc, ifaceptr, handler_inst, handler_func, post) \
	SH_ADD_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)

#define SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, handler, post) \
	__SourceHook_FHRemove##ifacetype##ifacefunc((void*)SourceHook::implicit_cast<ifacetype*>(ifaceptr), \
	post, handler)
#define SH_REMOVE_HOOK_STATICFUNC(ifacetype, ifacefunc, ifaceptr, handler, post) \
	SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_REMOVE_HOOK_MEMFUNC(ifacetype, ifacefunc, ifaceptr, handler_inst, handler_func, post) \
	SH_REMOVE_HOOK(ifacetype, ifacefunc, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)


#define SH_ADD_MANUALHOOK(hookname, ifaceptr, handler, post) \
	__SourceHook_FHMAdd##hookname(reinterpret_cast<void*>(ifaceptr), post, handler)
#define SH_ADD_MANUALHOOK_STATICFUNC(hookname, ifaceptr, handler, post) \
	SH_ADD_MANUALHOOK(hookname, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_ADD_MANUALHOOK_MEMFUNC(hookname, ifaceptr, handler_inst, handler_func, post) \
	SH_ADD_MANUALHOOK(hookname, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)

#define SH_REMOVE_MANUALHOOK(hookname, ifaceptr, handler, post) \
	__SourceHook_FHMRemove##hookname(reinterpret_cast<void*>(ifaceptr), post, handler)
#define SH_REMOVE_MANUALHOOK_STATICFUNC(hookname, ifaceptr, handler, post) \
	SH_REMOVE_MANUALHOOK(hookname, ifaceptr, fastdelegate::MakeDelegate(handler), post)
#define SH_REMOVE_MANUALHOOK_MEMFUNC(hookname, ifaceptr, handler_inst, handler_func, post) \
	SH_REMOVE_MANUALHOOK(hookname, ifaceptr, fastdelegate::MakeDelegate(handler_inst, handler_func), post)


#define SH_NOATTRIB




#if SH_COMP == SH_COMP_MSVC
# define SH_SETUP_MFP(mfp) \
	reinterpret_cast<void**>(&mfp)[0] = vfnptr_origentry;
#elif SH_COMP == SH_COMP_GCC
# define SH_SETUP_MFP(mfp) \
	reinterpret_cast<void**>(&mfp)[0] = vfnptr_origentry; \
	reinterpret_cast<void**>(&mfp)[1] = 0;
#else
# error Not supported yet.
#endif

//////////////////////////////////////////////////////////////////////////
#define SH_FHCls(ift, iff, ov) __SourceHook_FHCls_##ift##iff##ov
#define SH_MFHCls(hookname) __SourceHook_MFHCls_##hookname

#define SHINT_MAKE_HOOKMANPUBFUNC(ifacetype, ifacefunc, overload, funcptr) \
	SH_FHCls(ifacetype,ifacefunc,overload)() \
	{ \
		GetFuncInfo(funcptr, ms_MFI); \
	} \
	\
	static int HookManPubFunc(::SourceHook::HookManagerAction action, ::SourceHook::IHookManagerInfo *param) \
	{ \
		using namespace ::SourceHook; \
		GetFuncInfo(funcptr, ms_MFI); \
		/* Verify interface version */ \
		if (SH_GLOB_SHPTR->GetIfaceVersion() != SH_IFACE_VERSION) \
			return 1; \
		\
		if (action == HA_GetInfo) \
		{ \
			param->SetVersion(SH_HOOKMAN_VERSION); \
			param->SetInfo(ms_MFI.vtbloffs, ms_MFI.vtblindex, \
				reinterpret_cast<const char*>(&ms_Proto)); \
			\
			MemFuncInfo mfi; \
			GetFuncInfo(&SH_FHCls(ifacetype,ifacefunc,overload)::Func, mfi); \
			param->SetHookfuncVfnptr( \
				reinterpret_cast<void**>(reinterpret_cast<char*>(&ms_Inst) + mfi.vtbloffs)[mfi.vtblindex]); \
			return 0; \
		} \
		else if (action == HA_Register) \
		{ \
			ms_HI = param; \
			return 0; \
		} \
		else if (action == HA_Unregister) \
		{ \
			ms_HI = NULL; \
			return 0; \
		} \
		else \
			return 1; \
	}

// It has to be possible to use the macros in namespaces
// -> So we need to access and extend the global SourceHook namespace
// We use a namespace alias for this
#define SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, funcptr) \
	struct SH_FHCls(ifacetype,ifacefunc,overload) \
	{ \
		static SH_FHCls(ifacetype,ifacefunc,overload) ms_Inst; \
		static ::SourceHook::MemFuncInfo ms_MFI; \
		static ::SourceHook::IHookManagerInfo *ms_HI; \
		static ::SourceHook::ProtoInfo ms_Proto; \
		SHINT_MAKE_HOOKMANPUBFUNC(ifacetype, ifacefunc, overload, funcptr)

#define SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, funcptr) \
	}; \
	SH_FHCls(ifacetype,ifacefunc,overload) SH_FHCls(ifacetype,ifacefunc,overload)::ms_Inst; \
	::SourceHook::MemFuncInfo SH_FHCls(ifacetype,ifacefunc,overload)::ms_MFI; \
	::SourceHook::IHookManagerInfo *SH_FHCls(ifacetype,ifacefunc,overload)::ms_HI; \
	bool __SourceHook_FHAdd##ifacetype##ifacefunc(void *iface, bool post, \
		SH_FHCls(ifacetype,ifacefunc,overload)::FD handler) \
	{ \
		using namespace ::SourceHook; \
		MemFuncInfo mfi; \
		GetFuncInfo(funcptr, mfi); \
		if (mfi.thisptroffs < 0 || !mfi.isVirtual) \
			return false; /* No non-virtual functions / virtual inheritance supported */ \
		\
		return SH_GLOB_SHPTR->AddHook(SH_GLOB_PLUGPTR, iface, mfi.thisptroffs, \
			SH_FHCls(ifacetype,ifacefunc,overload)::HookManPubFunc, \
			new CSHDelegate<SH_FHCls(ifacetype,ifacefunc,overload)::FD>(handler), post); \
	} \
	bool __SourceHook_FHRemove##ifacetype##ifacefunc(void *iface, bool post, \
		SH_FHCls(ifacetype,ifacefunc,overload)::FD handler) \
	{ \
		using namespace ::SourceHook; \
		MemFuncInfo mfi; \
		GetFuncInfo(funcptr, mfi); \
		if (mfi.thisptroffs < 0) \
			return false; /* No virtual inheritance supported */ \
		\
		CSHDelegate<SH_FHCls(ifacetype,ifacefunc,overload)::FD> tmp(handler); \
		return SH_GLOB_SHPTR->RemoveHook(SH_GLOB_PLUGPTR, iface, mfi.thisptroffs, \
			SH_FHCls(ifacetype,ifacefunc,overload)::HookManPubFunc, &tmp, post); \
	} \

#define SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, pvtbloffs, pvtblidx, pthisptroffs) \
	struct SH_MFHCls(hookname) \
	{ \
		static SH_MFHCls(hookname) ms_Inst; \
		static ::SourceHook::MemFuncInfo ms_MFI; \
		static ::SourceHook::IHookManagerInfo *ms_HI; \
		static ::SourceHook::ProtoInfo ms_Proto; \
		\
		SH_MFHCls(hookname)() \
		{ \
			ms_MFI.isVirtual = true; \
			ms_MFI.thisptroffs = pthisptroffs; \
			ms_MFI.vtblindex = pvtblidx; \
			ms_MFI.vtbloffs = pvtbloffs; \
		} \
		static int HookManPubFunc(::SourceHook::HookManagerAction action, ::SourceHook::IHookManagerInfo *param) \
		{ \
			using namespace ::SourceHook; \
			/* we don't set ms_MFI here because manual hookmans can be reconfigured */ \
			/* :FIXME: possible problem: someone adding a hook from a constructor of a global entity */ \
			/* which is construced before SH_MFHCls(hookname)() gets called? */ \
			/* Verify interface version */ \
			if (SH_GLOB_SHPTR->GetIfaceVersion() != SH_IFACE_VERSION) \
				return 1; \
			\
			if (action == HA_GetInfo) \
			{ \
				param->SetVersion(SH_HOOKMAN_VERSION); \
				param->SetInfo(ms_MFI.vtbloffs, ms_MFI.vtblindex, \
					reinterpret_cast<const char*>(&ms_Proto)); \
				\
				MemFuncInfo mfi; \
				GetFuncInfo(&SH_MFHCls(hookname)::Func, mfi); \
				param->SetHookfuncVfnptr( \
					reinterpret_cast<void**>(reinterpret_cast<char*>(&ms_Inst) + mfi.vtbloffs)[mfi.vtblindex]); \
				return 0; \
			} \
			else if (action == HA_Register) \
			{ \
				ms_HI = param; \
				return 0; \
			} \
			else if (action == HA_Unregister) \
			{ \
				ms_HI = NULL; \
				return 0; \
			} \
			else \
				return 1; \
		}

#define SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, pvtbloffs, pvtblidx, pthisptroffs) \
	}; \
	SH_MFHCls(hookname) SH_MFHCls(hookname)::ms_Inst; \
	::SourceHook::MemFuncInfo SH_MFHCls(hookname)::ms_MFI; \
	::SourceHook::IHookManagerInfo *SH_MFHCls(hookname)::ms_HI; \
	bool __SourceHook_FHMAdd##hookname(void *iface, bool post, \
		SH_MFHCls(hookname)::FD handler) \
	{ \
		return SH_GLOB_SHPTR->AddHook(SH_GLOB_PLUGPTR, iface, pthisptroffs, \
			SH_MFHCls(hookname)::HookManPubFunc, \
			new ::SourceHook::CSHDelegate<SH_MFHCls(hookname)::FD>(handler), post); \
	} \
	bool __SourceHook_FHMRemove##hookname(void *iface, bool post, \
		SH_MFHCls(hookname)::FD handler) \
	{ \
		::SourceHook::CSHDelegate<SH_MFHCls(hookname)::FD> tmp(handler); \
		return SH_GLOB_SHPTR->RemoveHook(SH_GLOB_PLUGPTR, iface, pthisptroffs, \
			SH_MFHCls(hookname)::HookManPubFunc, &tmp, post); \
	} \


#define SH_SETUPCALLS(rettype, paramtypes, params) \
	/* 1) Find the vfnptr */ \
	using namespace ::SourceHook; \
	void *ourvfnptr = reinterpret_cast<void*>( \
		*reinterpret_cast<void***>(reinterpret_cast<char*>(this) + ms_MFI.vtbloffs) + ms_MFI.vtblindex); \
	IVfnPtr *vfnptr = ms_HI->FindVfnPtr(ourvfnptr); \
	SH_ASSERT(vfnptr, ("Called with vfnptr 0x%p which couldn't be found in the list", ourvfnptr)); \
	\
	void *vfnptr_origentry = vfnptr->GetOrigEntry(); \
	/* ... and the iface */ \
	IIface *ifinfo = vfnptr->FindIface(reinterpret_cast<void*>(this)); \
	if (!ifinfo) \
	{ \
		/* The iface info was not found. Redirect the call to the original function. */ \
		rettype (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		return (reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	} \
	/* 2) Declare some vars and set it up */ \
	SH_GLOB_SHPTR->HookLoopBegin(ifinfo); \
	IHookList *prelist = ifinfo->GetPreHooks(); \
	IHookList *postlist = ifinfo->GetPostHooks(); \
	META_RES status = MRES_IGNORED; \
	META_RES prev_res; \
	META_RES cur_res; \
	rettype orig_ret; \
	rettype override_ret; \
	rettype plugin_ret; \
	void* ifptr; \
	rettype *pOverrideRet = reinterpret_cast<rettype*>(SH_GLOB_SHPTR->SetupHookLoop( \
		&status, &prev_res, &cur_res, &ifptr, &orig_ret, &override_ret));

#define SH_CALL_HOOKS(post, params) \
	if (SH_GLOB_SHPTR->ShouldContinue()) \
	{ \
		prev_res = MRES_IGNORED; \
		for (AutoHookIter iter(post##list); !iter.End(); iter.Next()) \
		{ \
			cur_res = MRES_IGNORED; \
			ifptr = reinterpret_cast<void*>(reinterpret_cast<char*>(this) - iter.ThisPtrOffs()); \
			plugin_ret = reinterpret_cast<CSHDelegate<FD>*>(iter.Handler())->GetDeleg() params; \
			prev_res = cur_res; \
			if (cur_res > status) \
				status = cur_res; \
			if (cur_res >= MRES_OVERRIDE) \
				*pOverrideRet = plugin_ret; \
			if (!SH_GLOB_SHPTR->ShouldContinue()) \
			{ \
				iter.SetToZero(); \
				break; \
			} \
		} \
	}

#define SH_CALL_ORIG(rettype, paramtypes, params) \
	if (status != MRES_SUPERCEDE) \
	{ \
		rettype (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		orig_ret = (reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	} \
	else \
		orig_ret = override_ret; \

#define SH_RETURN() \
	SH_GLOB_SHPTR->HookLoopEnd(); \
	return status >= MRES_OVERRIDE ? *pOverrideRet : orig_ret;

#define SH_HANDLEFUNC(paramtypes, params, rettype) \
	SH_SETUPCALLS(rettype, paramtypes, params) \
	SH_CALL_HOOKS(pre, params) \
	SH_CALL_ORIG(rettype, paramtypes, params) \
	SH_CALL_HOOKS(post, params) \
	SH_RETURN()

//////////////////////////////////////////////////////////////////////////
#define SH_SETUPCALLS_void(paramtypes, params) \
	/* 1) Find the vfnptr */ \
	using namespace ::SourceHook; \
	void *ourvfnptr = reinterpret_cast<void*>( \
		*reinterpret_cast<void***>(reinterpret_cast<char*>(this) + ms_MFI.vtbloffs) + ms_MFI.vtblindex); \
	IVfnPtr *vfnptr = ms_HI->FindVfnPtr(ourvfnptr); \
	SH_ASSERT(vfnptr, ("Called with vfnptr 0x%p which couldn't be found in the list", ourvfnptr)); \
	\
	void *vfnptr_origentry = vfnptr->GetOrigEntry(); \
	/* ... and the iface */ \
	IIface *ifinfo = vfnptr->FindIface(reinterpret_cast<void*>(this)); \
	if (!ifinfo) \
	{ \
		/* The iface info was not found. Redirect the call to the original function. */ \
		void (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		(reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
		return; \
	} \
	/* 2) Declare some vars and set it up */ \
	SH_GLOB_SHPTR->HookLoopBegin(ifinfo); \
	IHookList *prelist = ifinfo->GetPreHooks(); \
	IHookList *postlist = ifinfo->GetPostHooks(); \
	META_RES status = MRES_IGNORED; \
	META_RES prev_res; \
	META_RES cur_res; \
	void* ifptr; \
	SH_GLOB_SHPTR->SetupHookLoop(&status, &prev_res, &cur_res, &ifptr, NULL, NULL); \

#define SH_CALL_HOOKS_void(post, params) \
	if (SH_GLOB_SHPTR->ShouldContinue()) \
	{ \
		prev_res = MRES_IGNORED; \
		for (AutoHookIter iter(post##list); !iter.End(); iter.Next()) \
		{ \
			cur_res = MRES_IGNORED; \
			ifptr = reinterpret_cast<void*>(reinterpret_cast<char*>(this) - iter.ThisPtrOffs()); \
			reinterpret_cast<CSHDelegate<FD>*>(iter.Handler())->GetDeleg() params; \
			prev_res = cur_res; \
			if (cur_res > status) \
				status = cur_res; \
			if (!SH_GLOB_SHPTR->ShouldContinue()) \
			{ \
				iter.SetToZero(); \
				break; \
			} \
		} \
	}

#define SH_CALL_ORIG_void(paramtypes, params) \
	if (status != MRES_SUPERCEDE) \
	{ \
		void (EmptyClass::*mfp)paramtypes; \
		SH_SETUP_MFP(mfp); \
		(reinterpret_cast<EmptyClass*>(this)->*mfp)params; \
	}

#define SH_RETURN_void() \
	SH_GLOB_SHPTR->HookLoopEnd();

#define SH_HANDLEFUNC_void(paramtypes, params) \
	SH_SETUPCALLS_void(paramtypes, params) \
	SH_CALL_HOOKS_void(pre, params) \
	SH_CALL_ORIG_void(paramtypes, params) \
	SH_CALL_HOOKS_void(post, params) \
	SH_RETURN_void()


// Special vafmt handlers
// :TODO: what
#define SH_HANDLEFUNC_vafmt(paramtypes, params_orig, params_plug, rettype) \
	SH_SETUPCALLS(rettype, paramtypes, params_orig) \
	SH_CALL_HOOKS(pre, params_plug) \
	SH_CALL_ORIG(rettype, paramtypes, params_orig) \
	SH_CALL_HOOKS(post, params_plug) \
	SH_RETURN()

#define SH_HANDLEFUNC_void_vafmt(paramtypes, params_orig, params_plug) \
	SH_SETUPCALLS_void(paramtypes, params_orig) \
	SH_CALL_HOOKS_void(pre, params_plug) \
	SH_CALL_ORIG_void(paramtypes, params_orig) \
	SH_CALL_HOOKS_void(post, params_plug) \
	SH_RETURN_void()

//////////////////////////////////////////////////////////////////////////

// ********* Support for 0 arguments *********
#define SH_DECL_HOOK0(ifacetype, ifacefunc, attr, overload, rettype) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)() attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate0<rettype> FD; \
		virtual rettype Func() \
		{ SH_HANDLEFUNC((), (), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)() attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0 }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		0, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK0_void(ifacetype, ifacefunc, attr, overload) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)() attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate0<> FD; \
		virtual void Func() \
		{ SH_HANDLEFUNC_void((), ()); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)() attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0 }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		0, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK0_vafmt(ifacetype, ifacefunc, attr, overload, rettype) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate1<const char *, rettype> FD; \
		virtual rettype Func(const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((...), ("%s", buf), (buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1 }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		0, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK0_void_vafmt(ifacetype, ifacefunc, attr, overload) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate1<const char *> FD; \
		virtual void Func(const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((...), ("%s", buf), (buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1 }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		0, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK0(hookname, vtblidx, vtbloffs, thisptroffs, rettype) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate0<rettype> FD; \
		virtual rettype Func() \
		{ SH_HANDLEFUNC((), (), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0 }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		0, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK0_void(hookname, vtblidx, vtbloffs, thisptroffs) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate0<> FD; \
		virtual void Func() \
		{ SH_HANDLEFUNC_void((), ()); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0 }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		0, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 1 arguments *********
#define SH_DECL_HOOK1(ifacetype, ifacefunc, attr, overload, rettype, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate1<param1, rettype> FD; \
		virtual rettype Func(param1 p1) \
		{ SH_HANDLEFUNC((param1), (p1), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		1, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK1_void(ifacetype, ifacefunc, attr, overload, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate1<param1> FD; \
		virtual void Func(param1 p1) \
		{ SH_HANDLEFUNC_void((param1), (p1)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		1, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK1_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate2<param1, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, ...), (p1, "%s", buf), (p1, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		1, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK1_void_vafmt(ifacetype, ifacefunc, attr, overload, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate2<param1, const char *> FD; \
		virtual void Func(param1 p1, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, ...), (p1, "%s", buf), (p1, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		1, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK1(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate1<param1, rettype> FD; \
		virtual rettype Func(param1 p1) \
		{ SH_HANDLEFUNC((param1), (p1), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		1, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK1_void(hookname, vtblidx, vtbloffs, thisptroffs, param1) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate1<param1> FD; \
		virtual void Func(param1 p1) \
		{ SH_HANDLEFUNC_void((param1), (p1)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		1, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 2 arguments *********
#define SH_DECL_HOOK2(ifacetype, ifacefunc, attr, overload, rettype, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate2<param1, param2, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2) \
		{ SH_HANDLEFUNC((param1, param2), (p1, p2), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		2, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK2_void(ifacetype, ifacefunc, attr, overload, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate2<param1, param2> FD; \
		virtual void Func(param1 p1, param2 p2) \
		{ SH_HANDLEFUNC_void((param1, param2), (p1, p2)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		2, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK2_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate3<param1, param2, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, ...), (p1, p2, "%s", buf), (p1, p2, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		2, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK2_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate3<param1, param2, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, ...), (p1, p2, "%s", buf), (p1, p2, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		2, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK2(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate2<param1, param2, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2) \
		{ SH_HANDLEFUNC((param1, param2), (p1, p2), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		2, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK2_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate2<param1, param2> FD; \
		virtual void Func(param1 p1, param2 p2) \
		{ SH_HANDLEFUNC_void((param1, param2), (p1, p2)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		2, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 3 arguments *********
#define SH_DECL_HOOK3(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate3<param1, param2, param3, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3) \
		{ SH_HANDLEFUNC((param1, param2, param3), (p1, p2, p3), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		3, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK3_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate3<param1, param2, param3> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3) \
		{ SH_HANDLEFUNC_void((param1, param2, param3), (p1, p2, p3)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		3, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK3_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, ...), (p1, p2, p3, "%s", buf), (p1, p2, p3, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		3, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK3_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, ...), (p1, p2, p3, "%s", buf), (p1, p2, p3, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		3, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK3(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate3<param1, param2, param3, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3) \
		{ SH_HANDLEFUNC((param1, param2, param3), (p1, p2, p3), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		3, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK3_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate3<param1, param2, param3> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3) \
		{ SH_HANDLEFUNC_void((param1, param2, param3), (p1, p2, p3)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		3, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 4 arguments *********
#define SH_DECL_HOOK4(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, param4, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4), (p1, p2, p3, p4), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		4, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK4_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, param4> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4), (p1, p2, p3, p4)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		4, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK4_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, ...), (p1, p2, p3, p4, "%s", buf), (p1, p2, p3, p4, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		4, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK4_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, ...), (p1, p2, p3, p4, "%s", buf), (p1, p2, p3, p4, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		4, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK4(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, param4, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4), (p1, p2, p3, p4), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		4, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK4_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate4<param1, param2, param3, param4> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4), (p1, p2, p3, p4)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		4, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 5 arguments *********
#define SH_DECL_HOOK5(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, param5, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5), (p1, p2, p3, p4, p5), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		5, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK5_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, param5> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5), (p1, p2, p3, p4, p5)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		5, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK5_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, ...), (p1, p2, p3, p4, p5, "%s", buf), (p1, p2, p3, p4, p5, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		5, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK5_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, ...), (p1, p2, p3, p4, p5, "%s", buf), (p1, p2, p3, p4, p5, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		5, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK5(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, param5, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5), (p1, p2, p3, p4, p5), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		5, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK5_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate5<param1, param2, param3, param4, param5> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5), (p1, p2, p3, p4, p5)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		5, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 6 arguments *********
#define SH_DECL_HOOK6(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, param6, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6), (p1, p2, p3, p4, p5, p6), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		6, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK6_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, param6> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6), (p1, p2, p3, p4, p5, p6)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		6, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK6_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, ...), (p1, p2, p3, p4, p5, p6, "%s", buf), (p1, p2, p3, p4, p5, p6, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		6, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK6_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, ...), (p1, p2, p3, p4, p5, p6, "%s", buf), (p1, p2, p3, p4, p5, p6, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		6, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK6(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, param6, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6), (p1, p2, p3, p4, p5, p6), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		6, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK6_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate6<param1, param2, param3, param4, param5, param6> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6), (p1, p2, p3, p4, p5, p6)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		6, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 7 arguments *********
#define SH_DECL_HOOK7(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, param7, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7), (p1, p2, p3, p4, p5, p6, p7), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		7, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK7_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, param7> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7), (p1, p2, p3, p4, p5, p6, p7)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		7, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK7_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, ...), (p1, p2, p3, p4, p5, p6, p7, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		7, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK7_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, ...), (p1, p2, p3, p4, p5, p6, p7, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		7, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK7(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, param7, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7), (p1, p2, p3, p4, p5, p6, p7), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		7, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK7_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate7<param1, param2, param3, param4, param5, param6, param7> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7), (p1, p2, p3, p4, p5, p6, p7)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		7, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 8 arguments *********
#define SH_DECL_HOOK8(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, param8, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8), (p1, p2, p3, p4, p5, p6, p7, p8), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		8, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK8_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, param8> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8), (p1, p2, p3, p4, p5, p6, p7, p8)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		8, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK8_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, ...), (p1, p2, p3, p4, p5, p6, p7, p8, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		8, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK8_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, ...), (p1, p2, p3, p4, p5, p6, p7, p8, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		8, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK8(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, param8, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8), (p1, p2, p3, p4, p5, p6, p7, p8), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		8, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK8_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate8<param1, param2, param3, param4, param5, param6, param7, param8> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8), (p1, p2, p3, p4, p5, p6, p7, p8)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		8, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 9 arguments *********
#define SH_DECL_HOOK9(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, param9, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9), (p1, p2, p3, p4, p5, p6, p7, p8, p9), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		9, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK9_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, param9> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9), (p1, p2, p3, p4, p5, p6, p7, p8, p9)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		9, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK9_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		9, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK9_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		9, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK9(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, param9, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9), (p1, p2, p3, p4, p5, p6, p7, p8, p9), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		9, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK9_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate9<param1, param2, param3, param4, param5, param6, param7, param8, param9> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9), (p1, p2, p3, p4, p5, p6, p7, p8, p9)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		9, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 10 arguments *********
#define SH_DECL_HOOK10(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		10, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK10_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		10, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK10_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		10, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK10_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		10, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK10(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		10, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK10_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate10<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		10, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 11 arguments *********
#define SH_DECL_HOOK11(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		11, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK11_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		11, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK11_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		11, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK11_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		11, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK11(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		11, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK11_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate11<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		11, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 12 arguments *********
#define SH_DECL_HOOK12(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		12, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK12_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		12, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK12_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		12, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK12_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		12, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK12(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		12, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK12_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate12<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		12, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 13 arguments *********
#define SH_DECL_HOOK13(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		13, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK13_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		13, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK13_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		13, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK13_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		13, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK13(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		13, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK13_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate13<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		13, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 14 arguments *********
#define SH_DECL_HOOK14(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		14, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK14_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		14, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK14_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		14, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK14_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		14, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK14(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		14, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK14_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate14<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		14, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 15 arguments *********
#define SH_DECL_HOOK15(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		15, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK15_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		15, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK15_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		15, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK15_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		15, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK15(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		15, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK15_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate15<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		15, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 16 arguments *********
#define SH_DECL_HOOK16(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		16, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK16_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		16, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK16_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		16, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK16_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		16, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK16(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		16, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK16_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate16<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		16, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 17 arguments *********
#define SH_DECL_HOOK17(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		17, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK17_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		17, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK17_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		17, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK17_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		17, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK17(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		17, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK17_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate17<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		17, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 18 arguments *********
#define SH_DECL_HOOK18(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		18, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK18_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		18, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK18_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		18, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK18_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		18, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK18(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		18, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK18_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate18<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		18, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 19 arguments *********
#define SH_DECL_HOOK19(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		19, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK19_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		19, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK19_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		19, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK19_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		19, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK19(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		19, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK19_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate19<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		19, __SourceHook_ParamSizesM_##hookname);

// ********* Support for 20 arguments *********
#define SH_DECL_HOOK20(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		20, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK20_void(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)); } \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) attr>(&ifacetype::ifacefunc))) \
	\
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		20, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK20_vafmt(ifacetype, ifacefunc, attr, overload, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate21<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, buf), rettype); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<rettype (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(sizeof(rettype), \
		20, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_HOOK20_void_vafmt(ifacetype, ifacefunc, attr, overload, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN(ifacetype, ifacefunc, overload, (static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *, ...) attr> \
		(&ifacetype::ifacefunc))) \
		typedef fastdelegate::FastDelegate21<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20, const char *fmt, ...) \
		{ \
			char buf[::SourceHook::STRBUF_LEN]; \
			va_list ap; \
			va_start(ap, fmt); \
			vsnprintf(buf, sizeof(buf), fmt, ap); \
			va_end(ap); \
			SH_HANDLEFUNC_void_vafmt((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, ...), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, "%s", buf), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20, buf)); \
		} \
	SHINT_MAKE_GENERICSTUFF_END(ifacetype, ifacefunc, overload, \
		(static_cast<void (ifacetype::*)(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, const char *, ...) attr>(&ifacetype::ifacefunc))) \
	\
	const int __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload[] = { -1, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_FHCls(ifacetype, ifacefunc, overload)::ms_Proto(0, \
		20, __SourceHook_ParamSizes_##ifacetype##ifacefunc##overload);

#define SH_DECL_MANUALHOOK20(hookname, vtblidx, vtbloffs, thisptroffs, rettype, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20, rettype> FD; \
		virtual rettype Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20) \
		{ SH_HANDLEFUNC((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20), rettype); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(sizeof(rettype), \
		20, __SourceHook_ParamSizesM_##hookname);

#define SH_DECL_MANUALHOOK20_void(hookname, vtblidx, vtbloffs, thisptroffs, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20) \
	SHINT_MAKE_GENERICSTUFF_BEGIN_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
		typedef fastdelegate::FastDelegate20<param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20> FD; \
		virtual void Func(param1 p1, param2 p2, param3 p3, param4 p4, param5 p5, param6 p6, param7 p7, param8 p8, param9 p9, param10 p10, param11 p11, param12 p12, param13 p13, param14 p14, param15 p15, param16 p16, param17 p17, param18 p18, param19 p19, param20 p20) \
		{ SH_HANDLEFUNC_void((param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14, param15, param16, param17, param18, param19, param20), (p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20)); } \
	SHINT_MAKE_GENERICSTUFF_END_MANUAL(hookname, vtbloffs, vtblidx, thisptroffs) \
	\
	const int __SourceHook_ParamSizesM_##hookname[] = { 0, sizeof(param1), sizeof(param2), sizeof(param3), sizeof(param4), sizeof(param5), sizeof(param6), sizeof(param7), sizeof(param8), sizeof(param9), sizeof(param10), sizeof(param11), sizeof(param12), sizeof(param13), sizeof(param14), sizeof(param15), sizeof(param16), sizeof(param17), sizeof(param18), sizeof(param19), sizeof(param20) }; \
	::SourceHook::ProtoInfo SH_MFHCls(hookname)::ms_Proto(0, \
		20, __SourceHook_ParamSizesM_##hookname);



//////////////////////////////////////////////////////////////////////////
// SH_CALL

#if SH_COMP == SH_COMP_MSVC

# define SH_MAKE_EXECUTABLECLASS_OB(call, prms) \
{ \
	using namespace ::SourceHook; \
	MemFuncInfo mfi; \
	GetFuncInfo(m_CC->GetThisPtr(), m_MFP, mfi); \
	void *origfunc = m_CC->GetOrigFunc(mfi.thisptroffs + mfi.vtbloffs, mfi.vtblindex); \
	if (!origfunc) \
		return (m_CC->GetThisPtr()->*m_MFP)call; \
	\
	/* It's hooked. Call the original function. */ \
	union \
	{ \
		RetType(EmptyClass::*mfpnew)prms; \
		void *addr; \
	} u; \
	u.addr = origfunc; \
	\
	void *adjustedthisptr = reinterpret_cast<void*>(reinterpret_cast<char*>(m_CC->GetThisPtr()) + mfi.thisptroffs); \
	return (reinterpret_cast<EmptyClass*>(adjustedthisptr)->*u.mfpnew)call; \
}

#elif SH_COMP == SH_COMP_GCC

# define SH_MAKE_EXECUTABLECLASS_OB(call, prms) \
{ \
	using namespace ::SourceHook; \
	MemFuncInfo mfi; \
	GetFuncInfo(m_CC->GetThisPtr(), m_MFP, mfi); \
	void *origfunc = m_CC->GetOrigFunc(mfi.thisptroffs + mfi.vtbloffs, mfi.vtblindex); \
	if (!origfunc) \
		return (m_CC->GetThisPtr()->*m_MFP)call; \
	\
	/* It's hooked. Call the original function. */ \
	union \
	{ \
		RetType(EmptyClass::*mfpnew)prms; \
		struct \
		{ \
			void *addr; \
			intptr_t adjustor; \
		} s; \
	} u; \
	u.s.addr = origfunc; \
	u.s.adjustor = mfi.thisptroffs; \
	\
	return (reinterpret_cast<EmptyClass*>(m_CC->GetThisPtr())->*u.mfpnew)call; \
}

#endif

namespace SourceHook
{
	template<class CCType, class RetType, class MFPType> class ExecutableClass
	{
		CCType *m_CC;
		MFPType m_MFP;
	public:
		ExecutableClass(CCType *cc, MFPType mfp) : m_CC(cc), m_MFP(mfp)
		{
		}

		// Support for 0 arguments
		RetType operator()() const
			SH_MAKE_EXECUTABLECLASS_OB((), ())

		// Support for 1 arguments
		template<class Param1> RetType operator()(Param1 p1) const
			SH_MAKE_EXECUTABLECLASS_OB((p1), (Param1))

		// Support for 2 arguments
		template<class Param1, class Param2> RetType operator()(Param1 p1, Param2 p2) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2), (Param1, Param2))

		// Support for 3 arguments
		template<class Param1, class Param2, class Param3> RetType operator()(Param1 p1, Param2 p2, Param3 p3) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3), (Param1, Param2, Param3))

		// Support for 4 arguments
		template<class Param1, class Param2, class Param3, class Param4> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4), (Param1, Param2, Param3, Param4))

		// Support for 5 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5), (Param1, Param2, Param3, Param4, Param5))

		// Support for 6 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6), (Param1, Param2, Param3, Param4, Param5, Param6))

		// Support for 7 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7), (Param1, Param2, Param3, Param4, Param5, Param6, Param7))

		// Support for 8 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8))

		// Support for 9 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9))

		// Support for 10 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10))

		// Support for 11 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11))

		// Support for 12 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12))

		// Support for 13 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13))

		// Support for 14 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14))

		// Support for 15 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15))

		// Support for 16 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15, Param16 p16) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16))

		// Support for 17 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15, Param16 p16, Param17 p17) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17))

		// Support for 18 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15, Param16 p16, Param17 p17, Param18 p18) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18))

		// Support for 19 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15, Param16 p16, Param17 p17, Param18 p18, Param19 p19) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19))

		// Support for 20 arguments
		template<class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20> RetType operator()(Param1 p1, Param2 p2, Param3 p3, Param4 p4, Param5 p5, Param6 p6, Param7 p7, Param8 p8, Param9 p9, Param10 p10, Param11 p11, Param12 p12, Param13 p13, Param14 p14, Param15 p15, Param16 p16, Param17 p17, Param18 p18, Param19 p19, Param20 p20) const
			SH_MAKE_EXECUTABLECLASS_OB((p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17, p18, p19, p20), (Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20))

	};
}

// SH_CALL needs to deduce the return type -> it uses templates and function overloading
// That's why SH_CALL takes two parameters: "mfp2" of type RetType(X::*mfp)(params), and "mfp" of type MFP
// The only purpose of the mfp2 parameter is to extract the return type

// Support for 0 arguments
template <class X, class Y, class MFP, class RetType>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)())
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)()const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 1 arguments
template <class X, class Y, class MFP, class RetType, class Param1>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 2 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 3 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 4 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 5 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 6 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 7 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 8 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 9 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 10 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 11 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 12 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 13 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 14 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 15 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 16 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 17 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 18 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 19 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 20 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}


#if SH_COMP != SH_COMP_MSVC || _MSC_VER > 1300
// GCC & MSVC 7.1 need this, MSVC 7.0 doesn't like it

// Support for 0 arguments
template <class X, class Y, class MFP, class RetType>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 1 arguments
template <class X, class Y, class MFP, class RetType, class Param1>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 2 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 3 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 4 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 5 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 6 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 7 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 8 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 9 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 10 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 11 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 12 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 13 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 14 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 15 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 16 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 17 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 18 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 19 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

// Support for 20 arguments
template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20, ...))
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}

template <class X, class Y, class MFP, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>
SH_CALL2(SourceHook::CallClass<Y> *ptr, MFP mfp, RetType(X::*mfp2)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20, ...)const)
{
	return SourceHook::ExecutableClass<SourceHook::CallClass<Y>, RetType, MFP>(ptr, mfp);
}


#endif

#define SH_CALL(ptr, mfp) SH_CALL2((ptr), (mfp), (mfp))

#undef SH_MAKE_EXECUTABLECLASS_OB

//////////////////////////////////////////////////////////////////////////
// SetOverrideRet and RecallGetIface for recalls
// These take a ISourceHook pointer instead of using SH_GLOB_SHPTR directly
// The reason is that the user may want to redefine SH_GLOB_SHPTR - then the macros
// (META_RETURN_VALUE_NEWPARAMS) should obey the new pointer.

namespace SourceHook
{
	template <class Iface, class RetType>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)())
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
	void SetOverrideResult(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20), const RetType res)
	{
		*reinterpret_cast<RetType*>(shptr->GetOverrideRetPtr()) = res;
	}

	template <class Iface, class RetType, class Param1, class Param2, class Param3, class Param4, class Param5, class Param6, class Param7, class Param8, class Param9, class Param10, class Param11, class Param12, class Param13, class Param14, class Param15, class Param16, class Param17, class Param18, class Param19, class Param20>
	Iface *RecallGetIface(ISourceHook *shptr, RetType (Iface::*mfp)(Param1, Param2, Param3, Param4, Param5, Param6, Param7, Param8, Param9, Param10, Param11, Param12, Param13, Param14, Param15, Param16, Param17, Param18, Param19, Param20))
	{
		return reinterpret_cast<Iface*>(shptr->GetIfacePtr());
	}
}

#endif
	// The pope is dead. -> :(
