////////////////////////////////////////////////////////////////////////////
//	Created		: 30.03.2010
//	Author		: Sergey Chechin
//	Copyright (C) GSC Game World - 2010
////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resources_manager.h"
#include "resources_callbacks.h"
#include "resources_device_manager.h"
#include "resources_allocators.h"

namespace xray {
namespace resources {

void   resources_manager::push_to_device_manager (query_result& query)
{
	fat_iterator	fat_it				=	wrapper_to_fat_it(query.get_fat_it());
	R_ASSERT								(!fat_it.is_end());

	device_manager *	const manager	=	query.find_capable_device_manager();
	manager->push_query						(&query);

	wakeup_resources_thread					();
}

void   resources_manager::allocate_resource_functionality::prepare_raw_resource_for_managed_or_unmanaged_cook (query_result * query)
{
	fat_iterator const fat_it						=	wrapper_to_fat_it(query->get_fat_it());

	if ( query->creation_data_from_user() )
	{
		// will use creation data instead of raw resource
		query->send_to_allocate_final_resource			();
	}
	else if ( fat_it.is_end() )
	{
		// will generate in final resource
		query->send_to_allocate_final_resource			();
	}
	else if ( fat_it.is_inlined() && !fat_it.is_compressed() )
	{
		// will use inline data instead of raw resource
		query->on_load_operation_end					();
	}
	else if ( fat_it.is_inlined() && fat_it.is_compressed() )
	{
		bool const allocated						=	query->allocate_raw_managed_resource_if_needed	();
		R_ASSERT_U										(allocated);
		query->on_load_operation_end					();
	}
	else if ( fat_it.is_compressed() )
	{		
		bool const allocated_compressed				=	query->allocate_compressed_resource_if_needed	();
		R_ASSERT_U										(allocated_compressed);
		g_resources_manager->on_allocated_raw_resource	(query);
	}
	else
	{
		g_resources_manager->on_allocated_raw_resource	(query);
	}
}

void   resources_manager::allocate_resource_functionality::prepare_raw_resource_for_inplace_managed_cook (query_result * query)
{
	fat_iterator const fat_it						=	wrapper_to_fat_it(query->get_fat_it());

	if ( query->creation_data_from_user() )
	{
		bool const allocated_raw					=	query->allocate_raw_managed_resource_if_needed	();
		R_ASSERT_U										(allocated_raw);
		bool const copied_data						=	query->copy_creation_data_to_resource_if_needed	();
		R_ASSERT_U										(copied_data);
		query->send_to_create_resource					();
	}
	else if ( fat_it.is_end() )
	{
		bool const allocated_raw					=	query->allocate_raw_managed_resource_if_needed	();
		R_ASSERT_U										(allocated_raw);
		query->send_to_create_resource					();		
	}
	else if ( fat_it.is_inlined() && !fat_it.is_compressed() )
	{
		bool const allocated_raw					=	query->allocate_raw_managed_resource_if_needed	();
		R_ASSERT_U										(allocated_raw);
		bool const copied_data						=	query->copy_inline_data_to_resource_if_needed	();
		R_ASSERT_U										(copied_data);
		query->send_to_create_resource					();
	}
	else if ( fat_it.is_inlined() && fat_it.is_compressed() )
	{
 		bool const allocated_raw					=	query->allocate_raw_managed_resource_if_needed	();
 		R_ASSERT_U										(allocated_raw);
		query->on_load_operation_end					();
	}
	else if ( fat_it.is_compressed() )
	{
		bool const allocated_compressed				=	query->allocate_compressed_resource_if_needed	();
		R_ASSERT_U										(allocated_compressed);

		g_resources_manager->on_allocated_raw_resource	(query);
	}
	else
	{
		g_resources_manager->on_allocated_raw_resource	(query);
	}
}

void   resources_manager::allocate_resource_functionality::prepare_raw_resource_for_inplace_unmanaged_cook (query_result * query)
{
	fat_iterator const fat_it						=	wrapper_to_fat_it(query->get_fat_it());

	if ( query->need_create_resource_inplace_in_creation_or_inline_data() )
	{
		query->bind_unmanaged_resource_buffer_to_creation_or_inline_data();
		query->send_to_create_resource					();
		return;
	}
	else if ( !fat_it.is_end() && query->is_compressed() && !fat_it.is_inlined() )
	{
		bool const allocated_compressed				=	query->allocate_compressed_resource_if_needed();
		R_ASSERT_U										(allocated_compressed);
	}

	if ( query->raw_unmanaged_buffer() )
	{
		continue_prepare_raw_resource_for_inplace_unmanaged_cook	(query);
		return;
	}
	
	u32 const allocate_thread_id					=	query->allocate_thread_id();
	if ( allocate_thread_id == threading::current_thread_id() )
	{
		bool const allocated						=	query->allocate_raw_unmanaged_resource_if_needed	();
		R_ASSERT_U										(allocated);
		continue_prepare_raw_resource_for_inplace_unmanaged_cook	(query);
		return;
	}

	thread_local_data * const local_data			=	g_resources_manager->get_thread_local_data(allocate_thread_id, true);
	local_data->to_allocate_raw_resource.push_back		(query);
	if ( allocate_thread_id == g_resources_manager->cooker_thread_id() )
		g_resources_manager->wakeup_cooker_thread		();
}

void   resources_manager::allocate_resource_functionality::continue_prepare_raw_resource_for_inplace_unmanaged_cook (query_result * query)
{
	fat_iterator const fat_it						=	wrapper_to_fat_it(query->get_fat_it());

	if ( query->creation_data_from_user() )
	{
		bool const copied_data						=	query->copy_creation_data_to_resource_if_needed	();
		R_ASSERT_U										(copied_data);
		query->send_to_create_resource					();
	}
	else if ( fat_it.is_end() )
	{
		query->send_to_create_resource					();		
	}
	else if ( fat_it.is_inlined() && !fat_it.is_compressed() )
	{
		bool const copied_data						=	query->copy_inline_data_to_resource_if_needed	();
		R_ASSERT_U										(copied_data);
		query->send_to_create_resource					();
	}
	else if ( fat_it.is_inlined() && fat_it.is_compressed() )
	{
		query->on_load_operation_end					();
	}
	else if ( fat_it.is_compressed() )
	{
		g_resources_manager->on_allocated_raw_resource	(query);
	}
	else
	{
		g_resources_manager->on_allocated_raw_resource	(query);
	}
}

void   resources_manager::allocate_resource_functionality::prepare_raw_resource_impl (query_result * query)
{
	class_id const class_id					=	query->get_class_id();
	if ( cook_base::find_managed_cook(class_id) || 
		 cook_base::find_unmanaged_cook(class_id) )
	{
		prepare_raw_resource_for_managed_or_unmanaged_cook		(query);
	}
	else if ( cook_base::find_inplace_managed_cook(class_id) ||
			 !cook_base::find_cook(class_id) )
	{
		prepare_raw_resource_for_inplace_managed_cook			(query);
	}
	else if ( cook_base::find_inplace_unmanaged_cook(class_id) )
	{
		prepare_raw_resource_for_inplace_unmanaged_cook			(query);
	}
	else
		NOT_IMPLEMENTED();
}

void   resources_manager::allocate_resource_functionality::send_to_allocate_final_resource_impl (query_result * const query)
{
	R_ASSERT									(threading::current_thread_id() == g_resources_manager->resources_thread_id());
	class_id const class_id					=	query->get_class_id();
	if ( cook_base::find_managed_cook(class_id) )
	{
		if ( threading::current_thread_id() == g_resources_manager->resources_thread_id() )
		{
			bool const allocation_result	=	query->allocate_final_managed_resource_if_needed();
			R_ASSERT_U							(allocation_result);
			query->on_allocated_final_resource	();
			return;
		}

		m_queries_to_allocate_managed_resource.push_back	(query);		
		g_resources_manager->wakeup_resources_thread		();
		return;
	}
	else if ( unmanaged_cook * const cook = cook_base::find_unmanaged_cook(class_id) )
	{
		u32 const allocate_thread_id		=	query->allocate_thread_id();
		if ( allocate_thread_id == threading::current_thread_id() )
		{
			bool const allocated			=	query->allocate_final_unmanaged_resource_if_needed();
			R_ASSERT_U						(allocated);
			query->on_allocated_final_resource	();
			return;
		}

		thread_local_data * const tls		=	g_resources_manager->get_thread_local_data(allocate_thread_id, true);

		tls->to_allocate_resource.push_back		(query);
		if ( allocate_thread_id == g_resources_manager->cooker_thread_id() )
			g_resources_manager->wakeup_cooker_thread	();

		return;
	}
	else
		NOT_IMPLEMENTED();
}

void   resources_manager::allocate_resource_functionality::tick (bool finalizing_thread)
{
	if ( threading::current_thread_id() == g_resources_manager->resources_thread_id() )
	{
		allocate_final_resources				(m_queries_to_allocate_managed_resource, finalizing_thread);
	}
	else
	{
		thread_local_data * const tls		=	g_resources_manager->get_thread_local_data(threading::current_thread_id(), false);
		R_ASSERT								(tls);
		
		allocate_final_resources				(tls->to_allocate_resource, finalizing_thread);
		allocate_raw_resources					(tls, finalizing_thread);
	}
}

template <class query_list>
void   resources_manager::allocate_resource_functionality::allocate_final_resources (query_list & queries, bool finalizing_thread)
{
	query_result * it_query					=	queries.pop_all_and_clear(); 
	bool allocating_managed					=	(& queries == & m_queries_to_allocate_managed_resource);

	while ( it_query )
	{
		query_result * const next_query		=	queries.get_next_of_object(it_query);
		
		if ( finalizing_thread )
		{
			it_query->set_error_type			(query_result::error_type_canceled_by_finalization);
			it_query->on_query_end				();
		}
		else
		{
			bool allocation_result			=	false;
			if ( allocating_managed )
				allocation_result			=	it_query->allocate_final_managed_resource_if_needed();
			else
				allocation_result			=	it_query->allocate_final_unmanaged_resource_if_needed();

			R_ASSERT							(allocation_result);

			it_query->on_allocated_final_resource	();
		}

		it_query							=	next_query;
	}
}

void   resources_manager::allocate_resource_functionality::allocate_raw_resources (thread_local_data * tls, bool finalizing_thread)
{
	bool set_event							=	false;

	query_result * it_query					=	tls->to_allocate_raw_resource.pop_all_and_clear();

	while ( it_query )
	{
		query_result* const next_query		=	tls->to_allocate_raw_resource.get_next_of_object(it_query);
		if ( finalizing_thread )
		{
			it_query->set_error_type			(query_result::error_type_canceled_by_finalization);
			it_query->on_query_end				();
		}
		else
		{
			bool const allocated			=	it_query->allocate_raw_unmanaged_resource_if_needed	();
			R_ASSERT_U						(allocated);
			continue_prepare_raw_resource_for_inplace_unmanaged_cook(it_query);
		}

		it_query							=	next_query;
		set_event							=	true;
	}

	if ( set_event )
		g_resources_manager->wakeup_resources_thread	();
}

void   resources_manager::dispatch_allocated_raw_resources ()
{
	query_result * it_query				=	m_queries_with_allocated_raw_resources.pop_all_and_clear();

	while ( it_query )
	{
		query_result * const next		=	m_queries_with_allocated_raw_resources.get_next_of_object(it_query);
		push_to_device_manager				(* it_query);
		it_query						=	next;
	}
}

pvoid   resources_manager::allocate_unmanaged_memory (u32 size, pcstr type_name)
{
	return										XRAY_MALLOC_IMPL(memory::g_resources_unmanaged_allocator, size, type_name);
}

managed_resource *   resources_manager::allocate_managed_resource (u32 size)
{
	return										memory::g_resources_managed_allocator.allocate(size);
}

void   resources_manager::free_managed_resource (managed_resource * const resource)
{
	LOGI_INFO									("resources:resource", "deleted %s", resource->log_string().c_str());
	memory::g_resources_managed_allocator.deallocate	(resource);
}

} // namespace resources
} // namespace xray
