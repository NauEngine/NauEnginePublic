// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include <nau/app/application.h>
#include <daECS/core/entityId.h>
#include <daECS/core/ecsQuery.h>
#include <EASTL/deque.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector_map.h>
#include <daECS/core/event.h>
#include "daECS/core/internal/templates.h"
#include "daECS/core/internal/trackComponentAccess.h"
#include "daECS/core/internal/stackAllocator.h"
#include "daECS/core/internal/eventsDb.h"
#include "daECS/core/internal/circularBuffer.h"
#include "daECS/core/internal/inplaceKeySet.h"
#include <daECS/core/template.h>
#include <daECS/core/entityComponent.h>
#include <daECS/core/ecsGameRes.h>
#include <nau/utils/dag_hashedKeyMap.h>
#include <nau/utils/dag_oaHashNameMap.h>
#include <nau/threading/spin_lock.h>
#include <nau/threading/critical_section.h>
#include <nau/app/application.h>
#include <nau/threading/dag_atomic.h>

namespace ecs
{

static const char *nullstr = NULL; // legacy
// This function shall be implemented by external code (unsatisifed compile-time dependency)

struct ScheduledArchetypeComponentTrack;
struct ScheduledEidComponentTrack;
struct ArchetypesQuery;
struct ArchetypesEidQuery;
struct ResolvedQueryDesc;
struct SchemelessEvent;
struct Query;
struct UpdateStageInfo;
struct EntitySystemDesc;
struct ComponentsIterator;
struct NestedQueryRestorer;

typedef eastl::function<void(EntityId /*created_entity*/)> create_entity_async_cb_t;
typedef void (*replication_cb_t)(EntityId eid, component_index_t cidx);

class NAU_DAGORECS_EXPORT EntityManager
{
public:
  // sets replication callback. Will be called once per tick on changing of replicating (in template) component for entities that do
  // have reserved components(replication)
  void setReplicationCb(replication_cb_t cb);

  // sets ES execution order and which es to skip
  void setEsOrder(nau::ConstSpan<const char *> es_order, nau::ConstSpan<const char *> es_skip);

  // Essentially set filtering tags for further components creation
  void setFilterTags(nau::ConstSpan<const char *> tags);

  // Checks that EntityId is existing (it can be without archetype yet)
  bool doesEntityExist(EntityId e) const;

  // Call update functions of all registered systems for particular stage type
  void update(const UpdateStageInfo &);

  // This method shall be called once per frame or so.
  // It's might perform various delayed activities/bookkeeping like creating & destroying deferered entities,
  // sending delayed events, etc...
  void tick(bool flush_all = false);

  // This method performs delayed entityCreation/tracking, and if flush_all = true it ensures it all performed (i.e. queues are empty)
  void performDelayedCreation(bool flush_all = true);

  // This method flushes all tracked changes (i.e. might call EventComponentChanged ESes)
  void performTrackChanges(bool flush_all = false);

  // set Constrained multi-threading mode.
  //  Constrained MT mode is designed to allow several ES to work in parallel in threads
  // when ecs is in Constrained multi-threading mode:
  //  * async(Re)Create of entities can be called
  //  * delayed events will not be send
  //  * all sendEvent (except immediate) will be delayed/postponed
  //  * tracking of changes will be postponed
  //  * delayed creation of entities will be postponed
  //  * new components can not be added
  //  * resetESOrder can not be called
  //  The following requirements have to be met:
  //  * setConstrainedMTMode can not be called from other thread (validated)
  //  * tick can not be called (validated)
  //  * registerType, createComponent can not be called (partially validated)
  //  * mutable gets can not be called (not validated)
  //  * no data race (writing to same components as reading, and especially writing in other threads)
  //     this is currently NOT checked at all, but it can be
  // setConstrainedMTMode can be only called not within query or creation of Entities
  void setConstrainedMTMode(bool on);
  bool isConstrainedMTMode() const;


  // Enable mode where entities with reserved components assigned IDs within lowest 64K
  // Typically enabled on server in order to have identical eids for networking entities with client
  void setEidsReservationMode(bool on);

  // Send event to particular entity / all entities.
  // Actual time of delivery for this events is not determined (i.e. it might be spread over several frames in case of congestion)
  // Order of delivery is guaranteed to be FIFO (Queue)
  template <class EventType>
  void sendEvent(EntityId eid, EventType &&evt);
  void sendEvent(EntityId eid, Event &evt); // untyped, Event will be copied from with memcpy. it's actual sizeof has to be getLength()
  void sendEvent(EntityId eid, Event &&evt);
  void sendEvent(EntityId eid, SchemelessEvent &&evt);

  template <class EventType>
  void broadcastEvent(EventType &&evt);
  void broadcastEvent(Event &evt); // untyped, Event will be copied from with memcpy. it's actual sizeof has to be getLength()
  void broadcastEvent(Event &&evt);
  void broadcastEvent(SchemelessEvent &&evt);

  //
  template <class EventType>
  void dispatchEvent(EntityId eid, EventType &&evt); // if eid == INVALID_ENTITY_ID it is broadcast, otherwise it is unicast
  void dispatchEvent(EntityId eid, Event &evt);      // if eid == INVALID_ENTITY_ID it is broadcast, otherwise it is unicast.
  void dispatchEvent(EntityId eid, Event &&evt);     // if eid == INVALID_ENTITY_ID it is broadcast, otherwise it is unicast.

  // Same as above but events are guaranteed to be delivered at the time of call.
  // Usage of this methods is discouraged as it can't be optimized (e.g. spread over several frames in case of congestion)
  // Also ordering of this events are not determined.
  void sendEventImmediate(EntityId eid, Event &evt);
  void broadcastEventImmediate(Event &evt);
  void sendEventImmediate(EntityId eid, Event &&evt);
  void broadcastEventImmediate(Event &&evt);

  // if return not AR_OK, nothing was added
  TemplateDB::AddResult addTemplate(Template &&templ, nau::ConstSpan<const char *> *pnames = NULL);
  TemplateDB::AddResult addTemplate(Template &&templ, nau::ConstSpan<template_t> parentIds);

  // AR_DUP here is not an error, it means it was updated
  TemplateDB::AddResult updateTemplate(Template &&templ, nau::ConstSpan<const char *> *pnames = NULL,
    bool update_template_values = false // update_template_values == true, will update entities component values which are same as in
                                        // old template, to values from new template
  );
  enum class RemoveTemplateResult
  {
    NotFound,
    HasEntities,
    Removed
  };
  RemoveTemplateResult removeTemplate(const char *name);
  void addTemplates(TemplateRefs &trefs, uint32_t tag = 0); // merely TemplateDB::resolveTemplateParents(trefs); for (t:trefs)
                                                            // addTemplate(move(t))

  enum class UpdateTemplateResult
  {
    Added,
    Updated,
    Same,
    InvalidName,
    RemoveHasEntities,
    DifferentTag,
    InvalidParents,
    Removed,
    Unknown
  };
  bool updateTemplates(TemplateRefs &trefs, bool update_templ_values, uint32_t tag,
    eastl::function<void(const char *, EntityManager::UpdateTemplateResult)> cb);

  const TemplateDB &getTemplateDB() const;
  TemplateDB &getTemplateDB();
  EntityId getSingletonEntity(const HashedConstString hashed_name);         // todo: check if it can be replaced with template name
  EntityId getOrCreateSingletonEntity(const HashedConstString hashed_name); // todo: check if it can be replaced with template name

  // Schedule entity creation with given components
  // Actual creation will happen in some unspecified (usually pretty close) moment in the future
  // Sends EventEntityCreated to entity systems that registered for it
  // 'templ' - template name to build entity from
  // 'initializer' - optional instance-specific components
  // 'cb' - callback that executed when actual entity created
  // Return INVALID_ENTITY_ID if entity creation failed
  // Returned eid is valid but can't be used for getting components or executing ESes until actual entity creation happens.
  EntityId createEntityAsync(const char *templ_name, ComponentsInitializer &&initializer = ComponentsInitializer(),
    ComponentsMap &&map = ComponentsMap(), create_entity_async_cb_t &&cb = create_entity_async_cb_t());
  EntityId createEntityAsync(template_t templ_id, ComponentsInitializer &&initializer = ComponentsInitializer(),
    ComponentsMap &&map = ComponentsMap(), create_entity_async_cb_t &&cb = create_entity_async_cb_t());
  // Same as above but entity created synchronously. It's generally discouraged to use this,
  // but it still might usefull for creation of logical entities that doesn't need game resources
  EntityId createEntitySync(const char *templ_name, ComponentsInitializer &&initializer = ComponentsInitializer(),
    ComponentsMap &&map = ComponentsMap()); // creates entity base on template name
  EntityId createEntitySync(template_t, ComponentsInitializer&& initializer = ComponentsInitializer(),
                            ComponentsMap&& map = ComponentsMap());  // creates entity base on template id
  template <class T>
  requires std::same_as<template_t, decltype(T::getTemplateId())>
  EntityId createEntitySync(ComponentsInitializer&& initializer = ComponentsInitializer(),
                            ComponentsMap&& map = ComponentsMap())
  {
      return createEntitySync(T::getTemplateId(), std::move(initializer), std::move(map));
  }
  // ECS2.0 Compatibility
  EntityId createEntityAsync(const char *templ_name, ComponentsInitializer &&initializer, create_entity_async_cb_t &&cb)
  {
    return createEntityAsync(templ_name, eastl::move(initializer), ComponentsMap(), eastl::move(cb));
  }

  // Schedule compare & remove difference in components/components of already existing entity & new template.
  // (i.e. remove extra/add missing components/components).
  // Parameters:
  //  'from_eid' - already existing entity that need to be modified
  //  'templ_name' - name of new template to calculate difference with. Supports special syntax
  //                 "templ1"+"templ2" for automatic creation new templates (sum of "templ1" & "templ2")
  //  'cb' - callback that will be executed on actual entity re-created
  //  Returns INVALID_ENTITY_ID if something gone wrong (e.g. passed template not found) or 'from_eid' otherwise.
  //  Returned eid is valid but can't be used for getting components or executing ESes until actual creation happens.
  //  Warning: 'from_eid' Entity should not have components that exist in instance only (i.e. not in template),
  //  otherwise it will be removed.
  EntityId reCreateEntityFromAsync(EntityId from_eid, const char *templ_name,
    ComponentsInitializer &&initializer = ComponentsInitializer(), ComponentsMap &&map = ComponentsMap(),
    create_entity_async_cb_t &&cb = create_entity_async_cb_t());

  // Deferred Destroy entity by id.
  // Returns true if entity exist and is added to destroy list, and false otherwise
  // Will send EventEntityDesroyed to entity systems that registered for it, only when destroyed
  bool destroyEntityAsync(const EntityId &eid);
  bool destroyEntityAsync(EntityId &eid);

  // Get Template's name for given entity (or nullptr if entity not exist/just allocated)
  const char *getEntityTemplateName(ecs::EntityId) const;

  // this is slower then getEntityTemplateId, but it checks queued entities
  // it is also NOT guaranteed to return correct results, if recreation requries adding components that need resources
  // it also SHOULD NOT be needed, and we add it is we currently have flawed design of recreation
  // instead of add_list, remove_list we provide the 'destination'
  // because of that add_sub_template(eid, "B"), add_sub_template(eid, "C") will result in +C, not +B+C, as expected!
  // in order to fix that, entitiy templates should be redesigned to be set of sub_templates
  //  getFutureTemplate allows simple workaround
  const char *getEntityFutureTemplateName(ecs::EntityId) const;

  // inspection
  //  How many components given entity has? Return negative if entity doesn't exist
  int getNumComponents(EntityId eid) const;
  int getArchetypeNumComponents(archetype_t archetype) const;

  // Remove all entities and all entity systems
  void clear();

  // Get entity components iterator (with or without template ones)
  // Warning: DO NOT recreateEntity from within this iterator - it might get invalidated!
  ComponentsIterator getComponentsIterator(EntityId eid, bool including_templates = true) const;

  typedef eastl::pair<const char *, const EntityComponentRef> ComponentInfo;
  // cid is 0.. till getArchetypeNumComponents
  ecs::component_index_t getArchetypeComponentIndex(archetype_t archetype, uint32_t cid) const;
  EntityComponentRef getEntityComponentRef(EntityId eid, uint32_t cid) const;   // cid is 0.. till getNumComponents
  ComponentInfo getEntityComponentInfo(EntityId eid, uint32_t cid) const; // cid is 0.. till getNumComponents
  bool isEntityComponentSameAsTemplate(ecs::EntityId eid, const EntityComponentRef cref, uint32_t cid) const; // for inspection.
                                                                                                              // returns true, if cid
                                                                                                              // is same as in template
  bool isEntityComponentSameAsTemplate(EntityId eid, uint32_t cid) const; // for inspection. returns true, if cid is same as in
                                                                          // template

  const EntityComponentRef getComponentRef(EntityId eid, const HashedConstString name) const; // named for ecs20 compatibility
  //! not checked for non-pod types. named for ecs20 compatibility
  void set(EntityId eid, const HashedConstString name, ChildComponent &&);
  //! not checked for non-pod types. Won't assert if missing. named for ecs20 compatibility
  void setOptional(EntityId eid, const HashedConstString name, ChildComponent &&);

  const EntityComponentRef getComponentRef(EntityId eid, component_index_t) const;
  EntityComponentRef getComponentRefRW(EntityId eid, component_index_t);

  template <class T>
  bool is(EntityId eid, const HashedConstString name) const
  {
    return is<T>(name) && has(eid, name);
  } // legacy! to be removed.

  template <class T>
  const T *__restrict getNullable(EntityId eid, const HashedConstString name) const;
  template <class T>
  T *__restrict getNullableRW(EntityId eid, const HashedConstString name);

  template <class T, typename U = ECS_MAYBE_VALUE_T(T)>
  U get(EntityId eid, const HashedConstString name) const;

  // Note: we intentonally return value (instead of const reference) here, because 'def' might be bound to temp variable
  template <class T>
  T getOr(EntityId eid, const HashedConstString name, const T &def) const;

  const char *getOr(EntityId eid, const HashedConstString name, const char *def) const;

  template <class T>
  T &__restrict getRW(EntityId eid, const HashedConstString name); // dangerous, as it has to return something!

  template <class T>
  void set(EntityId eid, const HashedConstString name, const T &__restrict v);
  void set(EntityId eid, const HashedConstString name, const char *v);
  template <class T>
  void setOptional(EntityId eid, const HashedConstString name, const T &__restrict v); // won't assert if missing

  // fast versions (about 40% faster in release)
  // they skip hasmap lookup version, and type validation. One have to validate type when resolve component_index_t
  template <class T, typename U = ECS_MAYBE_VALUE_T(T)>
  U getFast(EntityId eid, const component_index_t cidx, const LTComponentList *__restrict list) const;
  template <class T, typename U = ECS_MAYBE_VALUE_T(T)>
  U getOrFast(EntityId eid, const component_index_t cidx, const T &__restrict def) const;
  template <class T>
  const T *__restrict getNullableFast(EntityId eid, const component_index_t name) const;

  template <class T>
  T &__restrict getRWFast(EntityId eid, const FastGetInfo name, const LTComponentList *__restrict list);
  template <class T>
  T *__restrict getNullableRWFast(EntityId eid, const FastGetInfo name);
  template <class T>
  void setOptionalFast(EntityId eid, const FastGetInfo name, const T &__restrict v); // won't assert if missing
  template <class T>
  void setFast(EntityId eid, const FastGetInfo name, const T &__restrict v, const LTComponentList *__restrict list);

  // template <typename T, typename = eastl::enable_if_t<eastl::is_rvalue_reference<T&&>::value, void>>

  template <typename T>
  typename eastl::enable_if<eastl::is_rvalue_reference<T &&>::value>::type set(EntityId eid, const HashedConstString name, T &&v);
  template <typename T>
  typename eastl::enable_if<eastl::is_rvalue_reference<T &&>::value>::type setOptional(EntityId eid, const HashedConstString name,
    T &&v); // won't assert if missing
  template <class T>
  bool is(const HashedConstString name) const;
  bool has(EntityId eid, const HashedConstString name) const;

  QueryId createQuery(const NamedQueryDesc &desc);
  void destroyQuery(QueryId);
  const component_index_t *queryComponents(QueryId) const; // invalid index means unresolved

  void setMaxUpdateJobs(uint32_t max_num_jobs); // TODO Uncomment or delete
  //uint32_t getMaxWorkerId() const; // this is inclusive! i.e. it can return setMaxUpdateJobs()/MAX_POSSIBLE_WORKERS_COUNT + 1!
  void setQueryUpdateQuant(const char *es, uint16_t min_quant);

  EntityManager();
  EntityManager(const EntityManager &from);
  ~EntityManager();
  void copyFrom(const EntityManager &from);
  // bool performQuery(EntityId eid, const NamedQueryDesc &desc, const query_cb_t &fun);//not yet implemented

  // for inspection
  const DataComponents &getDataComponents() const { return dataComponents; }
  const ComponentTypes &getComponentTypes() const { return componentTypes; }
  nau::ConstSpan<const EntitySystemDesc*> getSystems() const { return esList; } // active systems
  int getEntitySystemSize(uint32_t es); // just return total entities size for es in getSystems()

  uint32_t getQueriesCount() const { return (uint32_t)queryDescs.size(); }
  QueryId getQuery(uint32_t id) const;
  const char *getQueryName(QueryId h) const;
  int getQuerySize(QueryId query); // just return total entities size
  const BaseQueryDesc getQueryDesc(QueryId h) const;

  // Get total number of created entities
  int getNumEntities() const;

  void onEntitiesLoaded(nau::ConstSpan<EntityId> ents, bool all_or); // this can only be called by resource system
  bool isLoadingEntity(EntityId eid) const;                          // returns true, only if entity is alive and not yet fully loaded

  component_index_t createComponent(const HashedConstString name, type_index_t component_type,
    nau::Span<component_t> non_optional_deps, ComponentSerializer *io, component_flags_t flags);
  LTComponentList* getComponentLT(const HashedConstString name);
  type_index_t registerType(const HashedConstString name, uint16_t data_size, ComponentSerializer *io, ComponentTypeFlags flags,
    create_ctm_t ctm, destroy_ctm_t dtm, void *user = nullptr); // manual registration. will not overwrite existing

  void flushDeferredEvents(); // do not casually call this! do not call from thread!

  // end of protected members
  void dumpArchetypes(int max_a);
  bool dumpArchetype(uint32_t arch);
  size_t dumpMemoryUsage();                             // dumps to debug
  void setEsTags(nau::ConstSpan<const char *> es_tags); // sets acceptable tags. If not set, _all_ are acceptable

  // Get Template's name for given entity (or nullptr if entity not exist/just allocated)
  ecs::template_t getEntityTemplateId(ecs::EntityId) const;
  archetype_t getArchetypeByTemplateId(ecs::template_t template_id) const;
  const char *getTemplateName(ecs::template_t) const;

  EventsDB &getEventsDbMutable() { return eventDb; }
  const EventsDB &getEventsDb() const { return eventDb; }
  void enableES(const char *es_name, bool on);

  static inline entity_id_t make_eid(uint32_t index, uint32_t gen)
  {
      return index | (gen << ENTITY_INDEX_BITS);
  }

  template_t templateByName(const char* templ_name, EntityId eid = {});
  template_t instantiateTemplate(int id);
  const Template* buildTemplateByName(const char*);
  int buildTemplateIdByName(const char*);

  protected:
  

  // this defers events for loading entity. it doesn't have to be public, it is not used by anyone
  void dispatchEventImmediate(EntityId eid, Event& evt);  // if eid == INVALID_ENTITY_ID it is broadcast, otherwise it is unicast.

  TemplateDBInfo& getMutableTemplateDBInfo()
  {
      return templateDB.info();
  }
  uint32_t defragArchetypes();
  uint32_t defragTemplates();
  const component_index_t* replicatedComponentsList(template_t t, uint32_t& cnt) const
  {
      return templates.replicatedComponentsList(t, cnt);
  }
  const bool isReplicatedComponent(template_t t, component_index_t cidx) const
  {
      return templates.isReplicatedComponent(t, cidx);
  }
  void forceServerEidGeneration(EntityId);  // only for replication, create server eid on client

  EntityId hasSingletoneEntity(hash_str_t hash) const;
  EntityId hasSingletoneEntity(const char* n) const;

  protected:
  typedef uint16_t es_index_type;
  static bool is_son_of(const Template& t, const int p, const TemplateDB& db, int rec_depth = 0);
  TemplateDB& getMutableTemplateDB();  // not sure if we have to expose non-const version..
  void updateEntitiesWithTemplate(template_t oldT, template_t newTemp, bool update_templ_values);
  void removeTemplateInternal(const uint32_t* to_remove, const uint32_t cnt);
  template_t createTemplate(ComponentsMap&& initializer, const char* debug_name = "(unknown)", const ComponentsFlags* flags = nullptr);
  void resetEsOrder();
  void initCompileTimeQueries();
  friend void reset_es_order();
  bool validateInitializer(template_t templ, ComponentsInitializer& comp_init) const;

  void createEntityInternal(EntityId eid, template_t templId, ComponentsInitializer&& initializer, ComponentsMap&& map, create_entity_async_cb_t&& cb);

  friend EntityId createInstantiatedEntitySync(EntityManager&, const char*, ComponentsInitializer&&);

  bool collapseRecreate(ecs::EntityId eid, const char* templId, ComponentsInitializer& cinit, ComponentsMap& cmap, create_entity_async_cb_t&& cb);
  __forceinline bool getEntityArchetype(EntityId eid, int& idx, uint32_t& archetype) const;
  void* getEntityComponentDataInternal(EntityId eid, uint32_t cid, uint32_t& archetype) const;
  void scheduleTrackChangedNoMutex(EntityId eid, component_index_t cidx);
  void scheduleTrackChanged(EntityId eid, component_index_t cidx);
  void scheduleTrackChangedCheck(EntityId eid, uint32_t archetypeId, component_index_t cidx);
  bool archetypeTrackChangedCheck(uint32_t archetypeId, component_index_t cidx);
  struct EntityDesc;
  static constexpr int MAX_ONE_EID_QUERY_COMPONENTS = 96;
  bool fillEidQueryView(ecs::EntityId eid, EntityDesc ent, QueryId h, QueryView& __restrict qv);
  template <typename Fn>
  bool performEidQuery(ecs::EntityId eid, QueryId h, Fn&& fun, void* user_data);

  struct ESPayLoad;
  typedef void (*ESFuncType)(const ESPayLoad& evt, const QueryView& __restrict components);
  template <typename Cb>
  __forceinline void performSTQuery(const Query& __restrict pQuery, void* user_data, const Cb& fun);
  void performQueryES(QueryId h, ESFuncType fun, const ESPayLoad&, void* user_data, int min_quant = 0);            // use min_quant of more than 0
                                                                                                                   // for parallel for execution
                                                                                                                   // (each job will take at least
                                                                                                                   // min_quant of data)
  void performQueryEmptyAllowed(QueryId h, ESFuncType fun, const ESPayLoad&, void* user_data, int min_quant = 0);  // use min_quant of
                                                                                                                   // more than 0 for
                                                                                                                   // parallel for
                                                                                                                   // execution (each job
                                                                                                                   // will take at least
                                                                                                                   // min_quant of data)

  void performQuery(QueryId query, const query_cb_t& fun, void* user_data = nullptr, int min_quant = 0);  // use min_quant of more than 0
                                                                                                          // for parallel for execution
                                                                                                          // (each job will take ata least
                                                                                                          // min_quant of data)
  void performQuery(const Query& __restrict query, const query_cb_t& __restrict fun, void* user_data, int min_quant = 1);
  bool performMTQuery(const Query& __restrict query, const query_cb_t& __restrict fun, void* user_data, int min_quant);
  QueryCbResult performQueryStoppable(const Query& __restrict pQuery, const stoppable_query_cb_t& fun, void* user_data);
  QueryCbResult performQueryStoppable(QueryId query, const stoppable_query_cb_t& fun, void* __restrict user_data = nullptr);  // use
                                                                                                                              // min_quant
                                                                                                                              // of more
                                                                                                                              // than 0
                                                                                                                              // for
                                                                                                                              // parallel
                                                                                                                              // for
                                                                                                                              // execution
                                                                                                                              // (each job
                                                                                                                              // will take
                                                                                                                              // ata least
                                                                                                                              // min_quant
                                                                                                                              // of data)

  bool makeQuery(const char* name, const BaseQueryDesc& desc, ArchetypesQuery& arch_desc, Query& query, struct QueryContainer&);
  // flags, not values
  enum ResolvedStatus : uint8_t
  {
      NOT_RESOLVED = 0,
      FULLY_RESOLVED = 1,
      RESOLVED = 2,
      RESOLVED_MASK = 3
  };
  ResolvedStatus resolveQuery(uint32_t index, ResolvedStatus current_status, ResolvedQueryDesc& resDesc);
  bool makeArchetypesQuery(archetype_t first_archetype, uint32_t index, bool wasResolved);
  uint32_t fillQuery(const ArchetypesQuery& archDesc, struct QueryContainer&);
  struct CommittedQuery commitQuery(QueryId h, struct QueryContainer& ctx, const ArchetypesQuery& arch, uint32_t total);
  void sheduleArchetypeTracking(const ArchetypesQuery& archDesc);
  bool validateQueryDesc(const BaseQueryDesc&) const;

  typedef dag::HashedKeySet<uint64_t, uint64_t(0), oa_hashmap_util::MumStepHash<uint64_t> /* framemem_allocator TODO Alocators.*/> TrackedChangesTemp;
  typedef dag::HashedKeySet<uint64_t, uint64_t(0), oa_hashmap_util::MumStepHash<uint64_t>> TrackedChangeEid;
  typedef dag::HashedKeySet<uint32_t, uint32_t(0), oa_hashmap_util::MumStepHash<uint32_t>> TrackedChangeArchetype;

  eastl::bitvector<> queryScheduled;
  eastl::vector<uint32_t> trackQueryIndices;
  void convertArchetypeScheduledChanges();  // converts queryScheduled + trackQueryIndices -> archetypeTrackingQueue

  TrackedChangeEid eidTrackingQueue;
  nau::threading::SpinLock archetypeTrackingMutex, eidTrackingMutex;

  eastl::bitvector<> canBeReplicated;
  TrackedChangeArchetype archetypeTrackingQueue;
  replication_cb_t replicationCb = NULL;

  void onChangeEvents(const TrackedChangesTemp& process);
  void preprocessTrackedChange(EntityId eid, archetype_t archetype, component_index_t cidx, TrackedChangesTemp& process);
  bool trackChangedArchetype(uint32_t archetype, component_index_t cidx, component_index_t old_cidx, TrackedChangesTemp& process);
  void trackChanged(EntityId eid, component_index_t cidx, TrackedChangesTemp& process);

  template <class T, int const_csz = 1, bool use_ctm = false>
  void compare_data(TrackedChangesTemp& process, uint32_t arch, const uint32_t compOffset, const uint32_t oldCompOffset, component_index_t cidx, size_t csz = 1, const ComponentTypeManager* ctm = nullptr);
  unsigned trackScheduledChanges();

  EntityId allocateOneEid();
  EntityId allocateOneEidDelayed(bool delayed);

  void removeDataFromArchetype(const uint32_t archetype, const chunk_type_t chunkId, const uint32_t idInChunk);  // remove data, without
                                                                                                                 // calling to destructors

  template <typename FilterUsed>
  void destroyComponents(const uint32_t archetype, const chunk_type_t chunkId, const uint32_t idInChunk, FilterUsed filter);  // to get
                                                                                                                              // data
                                                                                                                              // (from
                                                                                                                              // loading
                                                                                                                              // entity as
                                                                                                                              // well)
  template <typename FilterUsed>
  void removeFromArchetype(const uint32_t archetype, const chunk_type_t chunkId, const uint32_t idInChunk, FilterUsed filter);  // remove
                                                                                                                                // data
                                                                                                                                // and
                                                                                                                                // call
                                                                                                                                // destroy
  friend class ChildComponent;

#if NAU_DEBUG
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE_PASS , for_write
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG , bool for_write
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE , true
    #define DAECS_EXTENSIVE_CHECK_FOR_READ , false
#else
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE_PASS
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG
    #define DAECS_EXTENSIVE_CHECK_FOR_WRITE
    #define DAECS_EXTENSIVE_CHECK_FOR_READ
#endif

  void* __restrict get(EntityId eid, const HashedConstString name, component_type_t type_name, uint32_t sz, component_index_t& __restrict cidx, uint32_t& __restrict archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG) const;
  void* __restrict get(EntityId eid, const component_index_t cidx, uint32_t sz, uint32_t& __restrict archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG) const;

  archetype_component_id componentIndexInEntityArchetype(EntityId eid, const component_index_t index) const;
  archetype_component_id componentIndexInEntityArchetype(EntityId eid, const HashedConstString name) const;

  struct alignas(size_t) EntityDesc  // 64 bit.
  {
      archetype_t archetype = INVALID_ARCHETYPE;        // 16bit
      chunk_type_t chunkId = 0;                         // 8 bit
      uint8_t generation = 0;                           // 8 bit
      template_t template_id = INVALID_TEMPLATE_INDEX;  // 16bit
      id_in_chunk_type_t idInChunk = 0;                 // 16bit
      void reset()
      {
          template_id = INVALID_TEMPLATE_INDEX;
          archetype = INVALID_ARCHETYPE;
#if NAU_DEBUG
          idInChunk = eastl::numeric_limits<id_in_chunk_type_t>::max();
#endif
      }
  };
  static_assert(sizeof(EntityDesc) == 8);

  struct EntitiesDescriptors
  {
      eastl::vector<EntityDesc> entDescs;  // we rarely add entities, and often compare with size
      // Not zero counter in eid.index() slot means that entity was scheduled for [re]creation or destruction. SoA to entDescs
      eastl::vector<uint8_t> currentlyCreatingEntitiesCnt;
      uint32_t totalSize = 0;
      uint32_t delayedAdded = 0;
      decltype(EntityDesc::generation) globalGen = 0;
      bool isCurrentlyCreating(uint32_t idx) const
      {
          return idx < entDescs.size() ? currentlyCreatingEntitiesCnt[idx] != 0 : true;
      }
      void decreaseCreating(uint32_t idx)
      {
          currentlyCreatingEntitiesCnt[idx]--;
      }
      void increaseCreating(uint32_t idx)
      {
          if(idx < entDescs.size())
              currentlyCreatingEntitiesCnt[idx]++;
      }
      void clear()
      {
          entDescs.clear();
          currentlyCreatingEntitiesCnt.clear();
          totalSize = delayedAdded = 0;
      }
      void addDelayed()
      {
          if(delayedAdded == 0)
              return;
          EntityDesc e;
          e.generation = globalGen;
          entDescs.resize(totalSize, e);
          currentlyCreatingEntitiesCnt.resize(totalSize, 1);
          DAECS_EXT_ASSERT(totalSize == entDescs.size());
          delayedAdded = 0;
      }
      uint32_t push_back_delayed()
      {
          delayedAdded++;  // is done under mutex

          // while it is done under mutex, and simultaneuos add is not possible,
          // size() can be used in other thread, so it is important it is always sane numbers
          const uint32_t current = interlocked_relaxed_load(totalSize);
          interlocked_relaxed_store(totalSize, current + 1);
          return current;
      }
      uint32_t push_back()
      {
          addDelayed();
          const uint32_t idx = totalSize;
          entDescs.push_back();
          currentlyCreatingEntitiesCnt.push_back(0);
          totalSize++;
          DAECS_EXT_FAST_ASSERT(entDescs.size() == currentlyCreatingEntitiesCnt.size());
          return idx;
      }
      EntityId makeEid(uint32_t idx)
      {
          return EntityId(make_eid(idx, idx < allocated_size() ? entDescs[idx].generation : globalGen));
      }
      __forceinline bool doesEntityExist(unsigned idx, decltype(EntityDesc::generation) generation) const
      {
          return idx < size() && generation == (idx < allocated_size() ? entDescs[idx].generation : globalGen);
      }
      bool doesEntityExist(EntityId e) const
      {
          return doesEntityExist(e.index(), e.generation());
      }

      __forceinline bool getEntityArchetype(EntityId eid, int& idx, uint32_t& archetype) const
      {
          idx = eid.index();
          if(idx >= size())
              return false;
          if(idx >= allocated_size())
          {
              archetype = INVALID_ARCHETYPE;
              return false;
          }
          const auto& entDesc = entDescs[idx];
          if(entDesc.generation != eid.generation())
              return false;
          archetype = entDesc.archetype;
          return archetype != INVALID_ARCHETYPE;
      }

      enum class EntityState
      {
          Alive,
          Dead,
          Loading
      };
      inline EntityState getEntityState(EntityId eid) const
      {
          const unsigned idx = eid.index();
          if(idx >= size())  // it is dead
              return EntityState::Dead;
          if(idx >= allocated_size())  // it is not spawned
              return eid.generation() == globalGen ? EntityState::Loading : EntityState::Dead;
          const EntityDesc& entDesc = entDescs[idx];
          if(entDesc.generation != eid.generation())
              return EntityState::Dead;
          if(entDesc.archetype != INVALID_ARCHETYPE)  // dead or loaded
              return EntityState::Alive;
#if NAU_DEBUG
          if(currentlyCreatingEntitiesCnt[idx] == 0)
          {
              logerr(
                  "Entity {} isn't scheduled for creation, but has no archetype.\n"
                  "Most likely, reference to it in another entity was replicated from server, while entity itself was not.\n"
                  "Consider changing order of creation on server, scope sorting, or add explicit poll-and-wait before access to Entity",
                  eid);
          }
#endif
          return EntityState::Loading;
      }

      uint32_t allocated_size() const
      {
          return entDescs.size();
      }
      uint32_t size() const
      {
          return interlocked_relaxed_load(totalSize); 
      }
      uint32_t capacity() const
      {
          return entDescs.capacity();
      }
      void reserve(uint32_t a)
      {
          entDescs.reserve(a);
          currentlyCreatingEntitiesCnt.reserve(a);
      }
      void resize(uint32_t a)
      {
          EntityDesc e;
          e.generation = globalGen;
          entDescs.clear();
          entDescs.resize(a, e);

          currentlyCreatingEntitiesCnt.clear();
          currentlyCreatingEntitiesCnt.resize(a, 0);
          delayedAdded = 0;
          totalSize = a;
      }

      EntityDesc& operator[](uint32_t i)
      {
          return entDescs[i];
      }
      const EntityDesc& operator[](uint32_t i) const
      {
          return entDescs[i];
      }
  } entDescs;

  int constrainedMode = 0;
  int nestedQuery = 0;  // checked simultaneous with constrainedMode, so keep it close
  bool eidsReservationMode = false;
  bool exhaustedReservedIndices = false;
  uint32_t nextResevedEidIndex = 0;
  float lastTrackedCount = 0;
  eastl::deque<ecs::entity_id_t> freeIndices, freeIndicesReserved;
  Archetypes archetypes;
  ComponentTypes componentTypes;
  DataComponents dataComponents;
  Templates templates;
  eastl::unique_ptr<uint8_t[]> zeroMem;  // size of biggest type registered, so we can always return something in case of getNullableRW
  template <typename T>
  static inline T& get_scratch_value_impl(void* mem, eastl::true_type)
  {
      // Note: this will leak memory for types that hold memory (strings, vectors, objects...) if someone will write to it (since this
      // instance is never freed) but that's should be relatively ok since this is already emergency code-path
      return *new (mem /*, _NEW_INPLACE*/) T();
  }
  template <typename T>
  static inline T& get_scratch_value_impl(void* mem, eastl::false_type)
  {
      memset(mem, 0, sizeof(T));
      return *(T*)mem;
  }

  template <typename T>
  NAU_NOINLINE T& getScratchValue() const
  {
      typedef typename eastl::type_select<eastl::is_default_constructible<T>::value && !eastl::is_scalar<T>::value, eastl::true_type,
                                          eastl::false_type>::type TP;
      return get_scratch_value_impl<T>(zeroMem.get(), TP());
  }

  // during create get should work on entity currently being created.
  // if create will cause another immediate create, we try to survive that gracefully using stack of creating entities, although it is
  // potentially dangerous
  StackAllocator<> creationAllocator;
#if NAU_DEBUG
  struct CreatingEntity
  {
      EntityId eid;
      component_index_t createdCindex;
  };
  CreatingEntity creatingEntityTop;
#endif

  eastl::vector_map<ecs::EntityId, uint16_t> loadingEntities;  // it is vector_map eid->loadCount, as there can be more than one
                                                               // recreation scheduled

  RequestResources requestResources(EntityId eid, archetype_t old, template_t templId, const ComponentsInitializer& initializer, RequestResourcesType type);
  bool validateResources(EntityId eid, archetype_t old, template_t templId, const ComponentsInitializer& initializer);
  typename decltype(loadingEntities)::iterator find_loading_entity(EntityId eid)
  {
      return loadingEntities.find(eid);
  }
  typename decltype(loadingEntities)::const_iterator find_loading_entity(EntityId eid) const
  {
      return loadingEntities.find(eid);
  }
  void recreateOnSameArchetype(EntityId eid, template_t templId, ComponentsInitializer&& initializer, const create_entity_async_cb_t& cb);

  uint32_t lastEsGen = 0;
  ska::flat_hash_set<eastl::string> esTags;
  eastl::vector_map<eastl::string, uint32_t> esOrder;
  eastl::vector_set<eastl::string> disableEntitySystems, esSkip;
  eastl::vector<const EntitySystemDesc* /*, MidmemAlloc TODO Allocators.*/> esList;  // sorted
  eastl::bitvector<> esForAllEntities;

  eastl::vector<ecs::EntityId> resourceEntities;
  gameres_list_t requestedResources;

  friend void singleton_creation_event(const Event& evt, const QueryView& components);
  ska::flat_hash_map<hash_str_t, ecs::EntityId, ska::power_of_two_std_hash<hash_str_t>> singletonEntities;

  void flushGameResRequests();
  struct EsIndexFixedSet
  {
      const es_index_type* begin() const
      {
          return list.cbegin();
      }
      const es_index_type* end() const
      {
          return list.cend();
      }
      bool empty() const
      {
          return list.empty();
      }
      size_t size() const
      {
          return list.size();
      }
      size_t count(es_index_type value) const
      {
          return list.count(value);
      }
      void insert(es_index_type value)
      {
          list.insert(value);
      }

      typedef InplaceKeySet<es_index_type, 7, es_index_type> set_type_t;
      typedef typename set_type_t::shallow_copy_t shallow_copy_t;
      shallow_copy_t getShallowCopy() const
      {
          return list.getShallowCopy();
      }
      EsIndexFixedSet() = default;
      EsIndexFixedSet(const EsIndexFixedSet&) = delete;
      EsIndexFixedSet& operator=(const EsIndexFixedSet&) = delete;
#if NAU_DEBUG
      ~EsIndexFixedSet()
      {
          checkUnlocked("destructor", 0);
      }
      EsIndexFixedSet(EsIndexFixedSet&& a) :
          list(eastl::move(a.list)),
          lockedEid(a.lockedEid)
      {
          a.unlock();
      }
      EsIndexFixedSet& operator=(EsIndexFixedSet&& a)
      {
          list = eastl::move(a.list);
          eastl::swap(lockedEid, a.lockedEid);
          return *this;
      }
      void lock(EntityId eid) const
      {
          checkUnlocked("while locking entity", entity_id_t(eid));
          lockedEid = eid;
      }
      void unlock() const
      {
          lockedEid = EntityId{};
      }
      void checkUnlocked(const char* s, uint32_t value) const
      {
          if(lockedEid)
          {
              logerr("can't update es list {} {}, locked for processing eid={}", value, s, lockedEid);
          }
      }

  protected:
      mutable EntityId lockedEid;
#else
      EsIndexFixedSet(EsIndexFixedSet&& a) = default;
      EsIndexFixedSet& operator=(EsIndexFixedSet&&) = default;
      static __forceinline void lock(EntityId)
      {
      }
      static __forceinline void unlock()
      {
      }
      static __forceinline void checkUnlocked(const char*, uint32_t)
      {
      }
#endif
  protected:
      set_type_t list;
  };
  typedef EsIndexFixedSet es_index_set;
  eastl::vector<es_index_set> esUpdates;  // sorted by update priority ES functions (for each update stage)

  // probably use ska::flat_hash_map<event_type_t, event_index_t> esEventsMap; eastl::vector<eastl::vector_set<es_index_type>>
  // esEventsList; check performance
  ska::flat_hash_map<event_type_t, es_index_set, ska::power_of_two_std_hash<event_type_t>> esEvents;
  ska::flat_hash_map<component_t, es_index_set, ska::power_of_two_std_hash<event_type_t>> esOnChangeEvents;

  enum ArchEsList
  {
      ENTITY_CREATION_ES,
      ENTITY_RECREATION_ES,
      ENTITY_DESTROY_ES,
      ARCHETYPES_ES_LIST_COUNT
  };

  struct InternalEvent final : public Event
  {
      InternalEvent(event_type_t tp) :
          Event(tp, sizeof(Event), EVCAST_UNICAST | EVFLG_CORE)
      {
      }
  };

  InternalEvent archListEvents[ARCHETYPES_ES_LIST_COUNT] = {
      ECS_HASH("EventEntityCreated").hash, ECS_HASH("EventEntityRecreated").hash, ECS_HASH("EventEntityDestroyed").hash};

  enum ArchRecreateEsList
  {
      DISAPPEAR_ES,
      APPEAR_ES,
      RECREATE_ES_LIST_COUNT
  };
  InternalEvent recreateEvents[RECREATE_ES_LIST_COUNT] = {
      ECS_HASH("EventComponentsDisappear").hash, ECS_HASH("EventComponentsAppear").hash};
  typedef eastl::vector<es_index_set> archetype_es_list;
  archetype_es_list archetypesES[ARCHETYPES_ES_LIST_COUNT];

  struct RecreateEsSet
  {
      es_index_set disappear, appear;
  };
  typedef eastl::vector_map<archetype_t, RecreateEsSet> archetype_es_map;  // this is pointer, and not value, so we can add archetypes
                                                                           // within recreate ES call

  eastl::vector<archetype_es_map> archetypesRecreateES;  // same size as archetypesES. Make tuple vector?
  const RecreateEsSet* updateRecreatePair(archetype_t old_archetype, archetype_t new_archetype);

  void updateArchetypeESLists(archetype_t arch);
  void rebuildArchetypeESLists();
  void validateArchetypeES(archetype_t archetype);
  bool doesEsApplyToArch(es_index_type esIndex, archetype_t archetype);
  bool doesQueryIdApplyToArch(QueryId queryId, archetype_t archetype);

  const EntitySystemDesc& getESDescForEvent(es_index_type esIndex, const Event& evt) const;
  void registerEsEvents();
  void registerEsStage(int esId);
  void registerEsEvent(int esId);
  void registerEs(int esId);
  void validateEventRegistrationInternal(const Event& evt, const char* name);
#if NAU_DEBUG
  void validateEventRegistration(const Event& evt, const char* name)
  {
      validateEventRegistrationInternal(evt, name);
  }
#else
  void validateEventRegistration(const Event&, const char*)
  {
  }
#endif

  void callESEvent(es_index_type esIndex, const Event& evt, QueryView& qv);
  void notifyESEventHandlers(EntityId eid, const Event& evt);
  void notifyESEventHandlersInternal(EntityId eid, const Event& evt, const es_index_type* __restrict es_start, const es_index_type* __restrict es_end);
  void notifyESEventHandlers(EntityId eid, archetype_t archetype, ArchEsList list);

  __forceinline const RecreateEsSet* __restrict getRecreatePair(archetype_t old_archetype, archetype_t new_archetype)
  {
      const auto& archRecreateList = archetypesRecreateES[old_archetype];
      auto recreateListIt = archRecreateList.find(new_archetype);
      // update cache on the fly. If we hadn't ever recreated from old_archetype to new_archetype
      // ofc, we can make that N*(N-1) matrices, and remove updating cache on the fly, but there way less actual recreates.
      if(EASTL_UNLIKELY(recreateListIt == archetypesRecreateES[old_archetype].end()))
          return updateRecreatePair(old_archetype, new_archetype);
      else
          return &recreateListIt->second;
  }

  __forceinline void notifyESEventHandlersAppear(EntityId eid, archetype_t old_archetype, archetype_t new_archetype)
  {
      // we use shallow copy instead of just const reference, because unfortunately we still have createSync/instantiateTemplate inside ES
      // calls as long as it is removed - should not be needed anymore.
      auto& appearRef = getRecreatePair(old_archetype, new_archetype)->appear;
      if(!appearRef.empty())
      {
#if NAU_DEBUG
          appearRef.lock(eid);
#endif
          // we use shallow copy instead of just const reference, because unfortunately we still have createSync/instantiateTemplate inside
          // ES calls as long as it is removed - should not be needed anymore.
          auto appear = appearRef.getShallowCopy();
          notifyESEventHandlersInternal(eid, recreateEvents[APPEAR_ES], appear.cbegin(), appear.cend());
#if NAU_DEBUG
          getRecreatePair(old_archetype, new_archetype)->appear.unlock();  // we have to reget it, as it could be reallocated inside
#endif
      }
  }

  __forceinline void notifyESEventHandlersDisappear(EntityId eid, archetype_t old_archetype, archetype_t new_archetype)
  {
      auto& disappearRef = getRecreatePair(old_archetype, new_archetype)->disappear;
      if(!disappearRef.empty())
      {
#if NAU_DEBUG
          disappearRef.lock(eid);
#endif
          // we use shallow copy instead of just const reference, because unfortunately we still have createSync/instantiateTemplate inside
          // ES calls as long as it is removed - should not be needed anymore.
          auto disappear = disappearRef.getShallowCopy();
          notifyESEventHandlersInternal(eid, recreateEvents[DISAPPEAR_ES], disappear.cbegin(), disappear.cend());
#if NAU_DEBUG
          getRecreatePair(old_archetype, new_archetype)->disappear.unlock();  // we have to reget it, as it could be reallocated inside
#endif
      }
  }

  // eastl::hash_map<, ecs_query_handle> resolvedQueriesIndices;//combined NameQuery. list of all component_t + flags. has to be updated
  // if ecs_query_handle is completely destroyed
  bool isResolved(uint32_t idx) const;  // completely or partially
  bool isCompletelyResolved(uint32_t idx) const;
  bool updatePersistentQuery(archetype_t first_arch, QueryId id, uint32_t& index, bool should_re_resolve);
  bool updatePersistentQuery(archetype_t first_arch, QueryId id, bool should_re_resolve)
  {
      uint32_t index;
      return updatePersistentQuery(first_arch, id, index, should_re_resolve);
  }
  bool updatePersistentQueryInternal(archetype_t last_arch_count, uint32_t index, bool should_re_resolve);
  bool resolvePersistentQueryInternal(uint32_t index);
  bool makeArchetypesQueryInternal(archetype_t first_arch, uint32_t index, bool should_re_resolve);

  struct CopyQueryDesc
  {
#if NAU_DEBUG
      eastl::string name;
#endif
      eastl::vector<ComponentDesc> components;  // replace with FixedVector?
      uint8_t requiredSetCount = 0, rwCnt = 0, roCnt = 0, rqCnt = 0, noCnt = 0;
      // 3 bytes of padding. add is_eid_query?
      uint16_t rwEnd() const
      {
          return rwCnt;
      }
      uint16_t roEnd() const
      {
          return rwEnd() + roCnt;
      }
      uint16_t rqEnd() const
      {
          return roEnd() + rqCnt;
      }
      uint16_t noEnd() const
      {
          return rqEnd() + noCnt;
      }
      CopyQueryDesc() = default;
#if NAU_DEBUG
      const char* getName() const
      {
          return name.c_str();
      }
#else
      const char* getName() const
      {
          return "";
      }
#endif
      void setDebugName(const char* name_)
      {
          NAU_UNUSED(name_);
#if NAU_DEBUG
          name = name_;
#endif
      }
      void clear()
      {
      }
      void init(const EntityManager& mgr, const char* name_, const BaseQueryDesc& d);
      const BaseQueryDesc getDesc() const;
  };

  // SoA for QueryId
  uint8_t currentQueryGen = 0;
  void clearQueries();
  void invalidatePersistentQueries();
  struct QueryHasher
  {
      size_t operator()(const QueryId& h)
      {
          return wyhash64(uint32_t(h), 1);
      }
  };
  ska::flat_hash_map<QueryId, eastl::vector<es_index_type>, QueryHasher> queryToEsMap;
  QueryId createUnresolvedQuery(const NamedQueryDesc& desc);
  dag::OAHashNameMap<false> queriesComponentsNames;
#if NAU_DEBUG
#endif
  eastl::vector<ArchetypesQuery> archetypeQueries;        // SoA, we need to update ArchetypesQuery from ResolvedQueryDesc again, if we add new
                                                          // archetype
  eastl::vector<ArchetypesEidQuery> archetypeEidQueries;  // SoA, we need to update ArchetypesQuery from ResolvedQueryDesc again, if we add
                                                          // new archetype
  eastl::vector<archetype_t, EASTLAllocatorType> archSubQueriesContainer;
  uint32_t archSubQueriesWasted = 0;
  uint32_t archSubLastTickSize = 0;
  void defragmentArchetypeSubQueries();
  eastl::vector<uint16_t, EASTLAllocatorType> archComponentsSizeContainers;
  dag::Vector<ResolvedQueryDesc> resolvedQueries;  // SoA, we never need resolvedQuery again, if it was successfully resolved, but we need
                                                     // it to resolve ArchetypesQuery
  typedef uint32_t status_word_type_t;
  eastl::vector<status_word_type_t> resolvedQueryStatus;  // SoA, two-bit vector
  static constexpr status_word_type_t status_words_shift = nau::math::get_const_log2(sizeof(status_word_type_t) * 4);
  static constexpr status_word_type_t status_words_mask = (1 << status_words_shift) - 1;
  static bool isFullyResolved(ResolvedStatus s)
  {
      return s & FULLY_RESOLVED;
  }
  static bool isResolved(ResolvedStatus s)
  {
      return s != NOT_RESOLVED;
  }
  ResolvedStatus getQueryStatus(uint32_t idx) const
  {
      const status_word_type_t wordIdx = idx >> status_words_shift, wordShift = (idx & status_words_mask) << 1;
      return (ResolvedStatus)((resolvedQueryStatus[wordIdx] >> wordShift) & RESOLVED_MASK);
  }
  void orQueryStatus(uint32_t idx, ResolvedStatus status)
  {
      const uint32_t wordIdx = idx >> status_words_shift, wordShift = (idx & status_words_mask) << 1;
      resolvedQueryStatus[wordIdx] |= (status << wordShift);
  }
  void resetQueryStatus(uint32_t idx)
  {
      const status_word_type_t wordIdx = idx >> status_words_shift, wordShift = (idx & status_words_mask) << 1;
      resolvedQueryStatus[wordIdx] &= ~(status_word_type_t(RESOLVED_MASK) << wordShift);
  }
  void addOneResolvedQueryStatus()
  {
      uint32_t sz = (resolvedQueries.size() + status_words_mask) >> status_words_shift;
      if(sz != resolvedQueryStatus.size())
          resolvedQueryStatus.push_back(status_word_type_t(0));
  }
  eastl::vector<CopyQueryDesc> queryDescs;    // SoA, not empty ONLY if resolvedQueries is not resolved fully
  eastl::vector<uint16_t> queriesReferences;  // SoA, reference count of ecs_query_handles
  eastl::vector<uint8_t> queriesGenerations;  // SoA, generation in ecs_query_handles. Sanity check.
  uint32_t freeQueriesCount = 0;

  typedef uint64_t query_components_hash;
  static query_components_hash query_components_hash_calc(const BaseQueryDesc& desc);
  ska::flat_hash_map<query_components_hash, QueryId, ska::power_of_two_std_hash<query_components_hash>> queryMap;

  uint32_t addOneQuery();
  bool isQueryValidGen(QueryId id) const
  {
      if(!id)
          return false;
      auto idx = id.index();
      return idx < queriesGenerations.size() && queriesGenerations[idx] == id.generation();
  }
  bool isQueryValid(QueryId id) const
  {
      bool ret = isQueryValidGen(id);
      NAU_ASSERT(!ret || queriesReferences[id.index()]);
      return ret;
  }
  void queriesShrink();
  //--SoA for QueryId
  eastl::vector<QueryId> esListQueries;  // parallel to esList, index in ecs_query_handle

#if NAU_DEBUG
  EntityId destroyingEntity;
  bool isQueryingArchetype(uint32_t) const;
  void changeQueryingArchetype(uint32_t, int);
  struct ScopedQueryingArchetypesCheck
  {
      ScopedQueryingArchetypesCheck(uint32_t index_, EntityManager& mgr);
      ~ScopedQueryingArchetypesCheck()
      {
          changeQueryingArchetypes(-1);
      }
      EA_NON_COPYABLE(ScopedQueryingArchetypesCheck)
  private:
      EntityManager& mgr;
      uint32_t index, validateCount;
      void changeQueryingArchetypes(int add);
  };
#else
  bool isQueryingArchetype(uint32_t) const
  {
      return false;
  }
  void changeQueryingArchetype(uint32_t, int)
  {
  }
  struct ScopedQueryingArchetypesCheck
  {
      ScopedQueryingArchetypesCheck(uint32_t, EntityManager&)
      {
      }
  };
#endif
  bool isEventSendingPossible() const
  {
      return nestedQuery == 0 && !isConstrainedMTMode();
  }
  bool isDeferredCreationPossible() const
  {
      return nestedQuery == 0 && !isConstrainedMTMode();
  }

  uint32_t maxNumJobs = 0, maxNumJobsSet = 0;
  void updateCurrentUpdateMaxJobs();// TODO Uncomment or delete

  void destroyEntityImmediate(EntityId e);

  DeferredEventsStorage<> eventsStorage;
  uint32_t deferredEventsCount = 0;
  dag::CriticalSection deferredEventsMutex;
  template <class CircularBuffer, typename ProcessEvent>
  uint32_t processEventInternal(CircularBuffer& buffer, ProcessEvent&& cb);
  template <class EventStorage>
  uint32_t processEventsActive(uint32_t count, EventStorage&);
  template <class EventStorage>
  uint32_t processEventsAnyway(uint32_t count, EventStorage&);
  template <class EventStorage>
  void emplaceUntypedEvent(EventStorage& storage, EntityId eid, Event& evt);
  template <class T>
  void destroyEvents(T& storage);
  // should be out-of-line
  template <class EventStorage>
  uint32_t processEventsExhausted(uint32_t count, EventStorage&);

  struct ScopeSetMtConstrained
  {
      EntityManager& mgr;
      const bool wasConstrained;
      ScopeSetMtConstrained(EntityManager& m) :
          mgr(m),
          wasConstrained(m.isConstrainedMTMode())
      {
          if(!wasConstrained)
              mgr.setConstrainedMTMode(true);
      }
      ~ScopeSetMtConstrained()
      {
          if(!wasConstrained)
              mgr.setConstrainedMTMode(false);
      }
      EA_NON_COPYABLE(ScopeSetMtConstrained)
  };

  void performDeferredEvents(bool flush_all);  // manually call perform deferred events
  void debugInvalidateComponentes();

  struct LoadingEntityEvents
  {
      EntityId eid;
      DeferredEventsStorage<10> events;  // it is bigger_log2i(Event::max_event_size)
      bool operator<(const LoadingEntityEvents& rhs) const
      {
          return entity_id_t(eid) < entity_id_t(rhs.eid);
      }
      struct Compare
      {
          bool operator()(EntityId eid, const LoadingEntityEvents& rhs) const
          {
              return entity_id_t(eid) < entity_id_t(rhs.eid);
          }
          bool operator()(const LoadingEntityEvents& rhs, EntityId eid) const
          {
              return entity_id_t(rhs.eid) < entity_id_t(eid);
          }
      };
  };
  eastl::vector_set<LoadingEntityEvents> eventsForLoadingEntities;

  eastl::vector_set<LoadingEntityEvents>::iterator findLoadingEntityEvents(EntityId eid)
  {
      return eventsForLoadingEntities.find_as(eid, LoadingEntityEvents::Compare());
  }

  void addEventForLoadingEntity(EntityId eid, Event& evt);

  void sendQueuedEvents(uint32_t top_count_to);
  uint32_t current_tick_events = 0, average_tick_events_uint = 16;
  float average_tick_events = 16.f;

  mutable dag::CriticalSection creationMutex;
  template <typename T>
  struct ScopedMTMutexT
  {
      ScopedMTMutexT(bool is_mt, T& mutex_) :
          mutex(is_mt ? &mutex_ : nullptr)
      {
          if(mutex)
              mutex->lock();
          else
          {
              DAECS_EXT_ASSERT(nau::getApplication().isMainThread());
          }
      }
      ~ScopedMTMutexT()
      {
          if(mutex)
              mutex->unlock();
      }
      operator bool() const
      {
          return mutex != nullptr;
      }
      EA_NON_COPYABLE(ScopedMTMutexT);

  protected:
      T* mutex;
  };
  using ScopedMTMutex = ScopedMTMutexT<dag::CriticalSection>;

  struct DelayedEntityCreation
  {
      ComponentsInitializer compInit;
      ComponentsMap compMap;  // this one should probably be replaced with unique_ptr or something. todo: optimize it
      // We almost never have ComponentsMap, and it's size is 24 byte (pointer is just 8 bytes). But we have one(!) such component in each
      // network replicated object.
      create_entity_async_cb_t cb;
      eastl::string templateName;
      EntityId eid;
      template_t templ = INVALID_TEMPLATE_INDEX;
      enum class Op : uint8_t
      {
          Destroy,
          Create,
          Add,
          Sub
      } op = Op::Destroy;  // Add, Sub are unused currently but actually should be the only used!
      bool currentlyCreating = false;
      bool isToDestroy() const
      {
          return op == Op::Destroy;
      }
      DelayedEntityCreation(EntityId eid_, Op op_, const char* templ_name, ComponentsInitializer&& ai, ComponentsMap&& am, create_entity_async_cb_t&& cb_) :
          eid(eid_),
          op(op_),
          templateName(templ_name),
          compInit(eastl::move(ai)),
          compMap(eastl::move(am)),
          cb(eastl::move(cb_))
      {
          NAU_FAST_ASSERT(!isToDestroy());
      }
      DelayedEntityCreation(EntityId eid_, Op op_, template_t t, ComponentsInitializer&& ai, ComponentsMap&& am, create_entity_async_cb_t&& cb_) :
          eid(eid_),
          op(op_),
          templ(t),
          compInit(eastl::move(ai)),
          compMap(eastl::move(am)),
          cb(eastl::move(cb_))
      {
          NAU_FAST_ASSERT(!isToDestroy());
      }
      DelayedEntityCreation(EntityId eid_) :
          eid(eid_)
      {
      }
      void clear()
      {
          decltype(compInit)().swap(compInit);
          decltype(compMap)().swap(compMap);
          decltype(cb)().swap(cb);
          templateName.clear();
      }
  };
  struct DelayedEntityCreationChunk
  {
      static constexpr uint16_t minChunkCapacity = 64,  // no reason to create chunk with less than this number of entities
          maxChunkCapacity = SHRT_MAX + 1;
      DelayedEntityCreation* queue = nullptr;
      uint16_t readFrom = 0, writeTo = 0, capacity;
      DelayedEntityCreationChunk(uint16_t cap) :
          capacity(cap)
      {
          queue = (DelayedEntityCreation*)malloc(capacity * sizeof(DelayedEntityCreation) /*, defaultmem TODO Allocators.*/);
      }
      ~DelayedEntityCreationChunk()
      {
          for(auto i = begin(), e = end(); i != e; ++i)
              i->~DelayedEntityCreation();
          free(queue /*, defaultmem TODO Allocators.*/);
      }
      DelayedEntityCreationChunk(const DelayedEntityCreationChunk&) = delete;
      DelayedEntityCreationChunk& operator=(const DelayedEntityCreationChunk&) = delete;
      DelayedEntityCreationChunk(DelayedEntityCreationChunk&& a)
      {
          memcpy(this, &a, sizeof(DelayedEntityCreationChunk));
          memset(&a, 0, sizeof(DelayedEntityCreationChunk));
      }
      DelayedEntityCreationChunk& operator=(DelayedEntityCreationChunk&& a)
      {
          alignas(DelayedEntityCreationChunk) char buf[sizeof(DelayedEntityCreationChunk)];  // swap
          memcpy(buf, this, sizeof(DelayedEntityCreationChunk));
          memcpy(this, &a, sizeof(DelayedEntityCreationChunk));
          memcpy(&a, buf, sizeof(DelayedEntityCreationChunk));
          return *this;
      }
      DelayedEntityCreation* erase(DelayedEntityCreation* pos)
      {
          DelayedEntityCreation* end_ = end();
          // NAU_ASSERT(pos >= begin() && pos < end_, "{} {} {} {}", queue, pos, readFrom, writeTo);
          if((pos + 1) < end_)
              eastl::move(pos + 1, end_, pos);
          --writeTo;
          (end_ - 1)->~DelayedEntityCreation();
          return pos;
      }
      bool empty() const
      {
          return readFrom == writeTo;
      }
      bool full() const
      {
          return writeTo == capacity;
      }
      uint16_t size() const
      {
          return writeTo - readFrom;
      }
      uint16_t next_capacity() const
      {
          return std::min(capacity * 2, (int)maxChunkCapacity);
      }
      template <typename... Args>
      bool emplace_back(EntityId eid, Args&&... args)
      {
          DAECS_EXT_ASSERT(!full());
          new (queue + (writeTo++) /*, _NEW_INPLACE*/) DelayedEntityCreation(eid, eastl::forward<Args>(args)...);
          return full();
      }
      void reset()
      {
          readFrom = writeTo = 0;
      }
      const DelayedEntityCreation* end() const
      {
          return queue + writeTo;
      }
      const DelayedEntityCreation* begin() const
      {
          return queue + readFrom;
      }
      DelayedEntityCreation* end()
      {
          return queue + writeTo;
      }
      DelayedEntityCreation* begin()
      {
          return queue + readFrom;
      }
  };
  // we will always insert in top chunk, which is just always delayedCreationQueue.back();
  // which is never empty (i.e. topChunk != nullptr)
  eastl::vector<DelayedEntityCreationChunk> delayedCreationQueue;
  enum
  {
      INVALID_CREATION_QUEUE_GEN = 0,
      INITIAL_CREATION_QUEUE_GEN = 1
  };
  uint32_t createOrDestroyGen = INITIAL_CREATION_QUEUE_GEN + 1;
  uint32_t lastUpdatedCreationQueueGen = INITIAL_CREATION_QUEUE_GEN;
  bool someLoadedEntitiesHasErrors = false;

  template <typename... Args>
  void emplaceCreateInt(EntityId eid, Args&&... args)
  {
      DAECS_EXT_FAST_ASSERT(eid);
      DAECS_EXT_FAST_ASSERT(!delayedCreationQueue.empty());

#if DAGOR_THREAD_SANITIZER
      // why it is OK to shut tsan like that (but not in release mode)?
      //  the reason is simple. regardless of race in creation, if we are in constrainedMode,
      //   createOrDestroyGen is not ever READ, or WRITTEN, as isDeferredCreationPossible() == false
      //   so, this line is the only line that actually changes value of memory
      //  now, in any memory model, regardless of how many threads are simultaneously perform value++, value will CHANGE at least ONCE
      //  since we only use it to compare to lastUpdatedCreationQueueGen,
      //  which is only upated when isDeferredCreationPossible() == true to latest value of createOrDestroyGen
      //   we don't care if counter is actually representing amount of emplaceCreateInt called.
      //  it is good enough that createOrDestroyGen != lastUpdatedCreationQueueGen
      interlocked_increment(createOrDestroyGen);
#else
      createOrDestroyGen++;
#endif
      if(delayedCreationQueue.back().emplace_back(eid, eastl::forward<Args>(args)...))
          delayedCreationQueue.emplace_back(delayedCreationQueue.back().next_capacity());
  }

  template <typename... Args>
  void emplaceCreate(EntityId eid, Args&&... args)
  {
      emplaceCreateInt(eid, eastl::forward<Args>(args)...);
      entDescs.increaseCreating(eid.index());
  }
  void emplaceDestroy(ecs::EntityId eid)
  {
      emplaceCreateInt(eid);
  }
  void initializeCreationQueue()
  {
      DAECS_EXT_FAST_ASSERT(delayedCreationQueue.empty());
      delayedCreationQueue.emplace_back(+DelayedEntityCreationChunk::minChunkCapacity);
  }
  void clearCreationQueue()
  {
      delayedCreationQueue.clear();
      initializeCreationQueue();
      lastUpdatedCreationQueueGen = INITIAL_CREATION_QUEUE_GEN;
      createOrDestroyGen = INITIAL_CREATION_QUEUE_GEN;
  }
  bool createQueuedEntitiesOOL();  // return if new entites were added during creation
  inline bool hasQueuedEntitiesCreation()
  {
      // first comparison is 10x time faster than interlocked_acquire_load, so this order gives us performance
      // there is no actual race here!
      return lastUpdatedCreationQueueGen != interlocked_relaxed_load(createOrDestroyGen) && isDeferredCreationPossible();
  }
  inline bool createQueuedEntities()
  {
      return hasQueuedEntitiesCreation() ? createQueuedEntitiesOOL() : false;
  }  // return if new entites were added during creation

  TemplateDB templateDB;

  template <class T, bool optional>
  void setComponentInternal(EntityId eid, const HashedConstString name, const T& v);
  template <typename T, bool optional>
  typename eastl::enable_if<eastl::is_rvalue_reference<T&&>::value>::type setComponentInternal(EntityId eid,
                                                                                               const HashedConstString name,
                                                                                               T&& v);
  template <bool optional>
  void setComponentInternal(EntityId eid, const HashedConstString name, ChildComponent&&);  // not checked for non-pod types. Won't
                                                                                            // assert if missing. named for ecs20
                                                                                            // compatibility

  void allocateInvalid();
  uint32_t defragmentArchetypeId = 0;
  void validateQuery(uint32_t q);

  uint32_t allQueriesUpdatedToArch = 0;
  uint32_t lastQueriesResolvedComponents = 0;
  void updateAllQueriesInternal();
  bool updateAllQueriesAnyMT()
  {
      if(EASTL_LIKELY(allQueriesUpdatedToArch == archetypes.size()))
          return false;
      updateAllQueriesInternal();
      return true;
  }
  void updateAllQueries()
  {
      if(EASTL_UNLIKELY(!isConstrainedMTMode()))
          updateAllQueriesAnyMT();
  }

  void maintainQuery(uint32_t q);
  uint32_t queryToCheck = 0;
  void maintainQueries()
  {
      maintainQuery(queryToCheck++);
  }
  void defragmentArchetypes();
  template <typename Fn>
  friend bool perform_query(EntityManager*, ecs::EntityId eid, QueryId, Fn&&, void* a);
  template <typename Fn>
  friend bool perform_query(EntityManager*, ecs::EntityId eid, QueryId, Fn&&);
  friend QueryCbResult perform_query(EntityManager*, const NamedQueryDesc&, const stoppable_query_cb_t&, void*);
  friend QueryCbResult perform_query(EntityManager*, QueryId, const stoppable_query_cb_t&, void*);
  friend void perform_query(EntityManager*, const NamedQueryDesc&, const query_cb_t&, void*, int);
  friend void perform_query(EntityManager*, QueryId, const query_cb_t&, void*, int);

  void accessError(EntityId eid, const HashedConstString name) const;
  void accessError(EntityId eid, component_index_t cidx, const LTComponentList* list) const;
  mutable int errorCount = 0;
  EventsDB eventDb;

  friend class ResourceRequestCb;
  struct CurrentlyRequesting
  {
      EntityId eid;
      template_t newTemplate;
      archetype_t oldArchetype;
      archetype_t newArchetype;
      const ComponentsInitializer& initializer;
      CurrentlyRequesting(EntityId eid_, template_t newTemplate_, archetype_t oldArch, archetype_t newArch, const ComponentsInitializer& i) :
          initializer(i),
          eid(eid_),
          newTemplate(newTemplate_),
          newArchetype(newArch),
          oldArchetype(oldArch)
      {
      }
  };
  CurrentlyRequesting* requestingTop = nullptr;  // pointer to stack variable
  template <class T>
  const T* getRequestingBase(const HashedConstString name) const;
  template <class T>
  const T& getRequesting(const HashedConstString hashed_name) const;
  template <class T>
  const T& getRequesting(const HashedConstString hashed_name, const T& def) const;

  int getNestedQuery() const
  {
      return nestedQuery;
  }
  void setNestedQuery(int value)
  {
      nestedQuery = value;
  }
  friend struct ecs::NestedQueryRestorer;
  void schedule_tracked_changes(const ScheduledArchetypeComponentTrack* __restrict trackedI, uint32_t trackedChangesCount, EntityId eid, uint32_t archetype);
};

}; // namespace ecs

NAU_DAGORECS_EXPORT extern InitOnDemand<ecs::EntityManager, false> g_entity_mgr;

#define ECS_DECLARE_GET_FAST_BASE(type_, aname, aname_str) \
  static ecs::LTComponentList aname##_component(ECS_HASH(aname_str), ecs::ComponentTypeInfo<type_>::type, __FILE__, "", 0)
#define ECS_DECLARE_GET_FAST(type_, aname) ECS_DECLARE_GET_FAST_BASE(type_, aname, #aname)

#define ECS_GET_COMPONENT(type_, eid, aname) \
  g_entity_mgr->getNullableRW<typename eastl::remove_const<type_>::type>(eid, ECS_HASH(#aname))
#define ECS_GET_COMPONENT_RO(type_, eid, aname) \
  g_entity_mgr->getNullable<typename eastl::remove_const<type_>::type>(eid, ECS_HASH(#aname))

#if _ECS_CODEGEN


template <class T>
T &ecs_codegen_stub_type();
template <class T>
T &a_ecs_codegen_get_call(const char *, const char *);
template <class T>
T &or_a_ecs_codegen_set_call(const char *, T &&);
template <class T>
T *nullable_a_ecs_codegen_get_call(const char *, const char *);
template <class T>
void a_ecs_codegen_set_call(const char *, T &&);

#define ECS_GET(type_, eid, aname)          a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_GET_OR(eid, aname, def)         or_a_ecs_codegen_set_call(#aname, def)
#define ECS_GET_NULLABLE(type_, eid, aname) nullable_a_ecs_codegen_get_call<type_>(#aname, #type_)

#define ECS_GET_RW(type_, eid, aname)          a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_GET_NULLABLE_RW(type_, eid, aname) nullable_a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_SET(eid, aname, v)                 a_ecs_codegen_set_call(#aname, v)
#define ECS_SET_OPTIONAL(eid, aname, v)        a_ecs_codegen_set_call(#aname, v)

#define ECS_GET_MGR(mgr, type_, eid, aname)          a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_GET_OR_MGR(mgr, eid, aname, def)         or_a_ecs_codegen_set_call(#aname, def)
#define ECS_GET_NULLABLE_MGR(mgr, type_, eid, aname) nullable_a_ecs_codegen_get_call<type_>(#aname, #type_)

#define ECS_GET_RW_MGR(mgr, type_, eid, aname)          a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_GET_NULLABLE_RW_MGR(mgr, type_, eid, aname) nullable_a_ecs_codegen_get_call<type_>(#aname, #type_)
#define ECS_SET_MGR(mgr, eid, aname, v)                 a_ecs_codegen_set_call(#aname, v)
#define ECS_SET_OPTIONAL_MGR(mgr, eid, aname, v)        a_ecs_codegen_set_call(#aname, v)

#define ECS_INIT(to, aname, v) a_ecs_codegen_set_call(#aname, v)

#else

#define ECS_GET(type_, eid, aname)          g_entity_mgr->getFast<type_>(eid, aname##_component.getCidx(), &aname##_component)
#define ECS_GET_OR(eid, aname, def)         g_entity_mgr->getOrFast(eid, aname##_component.getCidx(), def)
#define ECS_GET_NULLABLE(type_, eid, aname) g_entity_mgr->getNullableFast<type_>(eid, aname##_component.getCidx())

#define ECS_GET_RW(type_, eid, aname)          g_entity_mgr->getRWFast<type_>(eid, aname##_component.getInfo(), &aname##_component)
#define ECS_GET_NULLABLE_RW(type_, eid, aname) g_entity_mgr->getNullableRWFast<type_>(eid, aname##_component.getInfo())
#define ECS_SET(eid, aname, v)                 g_entity_mgr->setFast(eid, aname##_component.getInfo(), v, &aname##_component)
#define ECS_SET_OPTIONAL(eid, aname, v)        g_entity_mgr->setOptionalFast(eid, aname##_component.getInfo(), v)

#define ECS_GET_MGR(mgr, type_, eid, aname)          mgr->getFast<type_>(eid, aname##_component.getCidx(), &aname##_component)
#define ECS_GET_OR_MGR(mgr, eid, aname, def)         mgr->getOrFast(eid, aname##_component.getCidx(), def)
#define ECS_GET_NULLABLE_MGR(mgr, type_, eid, aname) mgr->getNullableFast<type_>(eid, aname##_component.getCidx())

#define ECS_GET_RW_MGR(mgr, type_, eid, aname)          mgr->getRWFast<type_>(eid, aname##_component.getInfo(), &aname##_component)
#define ECS_GET_NULLABLE_RW_MGR(mgr, type_, eid, aname) mgr->getNullableRWFast<type_>(eid, aname##_component.getInfo())
#define ECS_SET_MGR(mgr, eid, aname, v)                 mgr->setFast(eid, aname##_component.getInfo(), v, &aname##_component)
#define ECS_SET_OPTIONAL_MGR(mgr, eid, aname, v)        mgr->setOptionalFast(eid, aname##_component.getInfo(), v)

#define ECS_INIT(to, aname, v) to.insert(g_entity_mgr->getComponentLT(ECS_HASH(aname))->getName(), g_entity_mgr->getComponentLT(ECS_HASH(aname))->getInfo(), v, g_entity_mgr->getComponentLT(ECS_HASH(aname))->getNameStr())

#endif

// ecs20 preprocessor macro, to be removed
#define ECS_GET_ENTITY_COMPONENT(type, var, eid, aname) const type *var = ECS_GET_NULLABLE(type, eid, aname)

#define ECS_GET_ENTITY_COMPONENT_RW(type, var, eid, aname) type *var = ECS_GET_NULLABLE_RW(type, eid, aname) //-V1003

#define ECS_GET_SINGLETON_COMPONENT(type_, aname) \
  g_entity_mgr->getNullableRW<type_>(g_entity_mgr->getSingletonEntity(ECS_HASH(#aname)), ECS_HASH(#aname))


// implementations
namespace ecs
{

    NAU_FORCE_INLINE bool EntityManager::isConstrainedMTMode() const
    {
        return interlocked_acquire_load(constrainedMode) != 0;
    }
    NAU_FORCE_INLINE const TemplateDB& EntityManager::getTemplateDB() const
    {
        return templateDB;
    }
    NAU_FORCE_INLINE TemplateDB& EntityManager::getTemplateDB()
    {
        return templateDB;
    }
    NAU_FORCE_INLINE TemplateDB& EntityManager::getMutableTemplateDB()
    {
        return templateDB;
    }  // thread unsafe!
    inline const Template* EntityManager::buildTemplateByName(const char* n)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        return templateDB.buildTemplateByName(n);
    }
    inline int EntityManager::buildTemplateIdByName(const char* n)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        return templateDB.buildTemplateIdByName(n);
    }
    inline TemplateDB::AddResult EntityManager::addTemplate(Template&& templ, nau::ConstSpan<const char*>* pnames)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        return templateDB.addTemplate(eastl::move(templ), pnames);
    }
    inline TemplateDB::AddResult EntityManager::addTemplate(Template&& templ, nau::ConstSpan<template_t> parentIds)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        return templateDB.addTemplate(eastl::move(templ), parentIds);
    }
    inline void EntityManager::addTemplates(TemplateRefs& trefs, uint32_t tag)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        templateDB.addTemplates(trefs, tag);
    }

    inline const BaseQueryDesc EntityManager::CopyQueryDesc::getDesc() const
    {
        return BaseQueryDesc(nau::ConstSpan<ComponentDesc>(components.begin(), rwCnt),
                             nau::ConstSpan<ComponentDesc>(components.begin() + rwEnd(), roCnt),
                             nau::ConstSpan<ComponentDesc>(components.begin() + roEnd(), rqCnt),
                             nau::ConstSpan<ComponentDesc>(components.begin() + rqEnd(), noCnt));
    }

    template <class T>
    inline bool EntityManager::is(const HashedConstString name) const
    {
        return dataComponents.findComponent(name.hash).componentTypeName == ComponentTypeInfo<T>::type;
    }

    NAU_FORCE_INLINE bool EntityManager::getEntityArchetype(EntityId eid, int& idx, uint32_t& archetype) const
    {
        const bool ret = entDescs.getEntityArchetype(eid, idx, archetype);
        if(ret)
        {
            DAECS_VALIDATE_ARCHETYPE(archetype);
        }
        return ret;
    }

    inline archetype_component_id EntityManager::componentIndexInEntityArchetype(EntityId eid, const component_index_t index) const
    {
        if(index == INVALID_COMPONENT_INDEX)
            return INVALID_ARCHETYPE_COMPONENT_ID;

        int idx;
        uint32_t archetype = INVALID_ARCHETYPE;
        if(!getEntityArchetype(eid, idx, archetype))
            return INVALID_ARCHETYPE_COMPONENT_ID;
        return archetypes.getArchetypeComponentId(archetype, index);
    }

    NAU_FORCE_INLINE archetype_component_id EntityManager::componentIndexInEntityArchetype(EntityId eid, const HashedConstString name) const
    {
#if DAGOR_DBGLEVEL > 1
        const char* componentName = dataComponents.findComponentName(name.hash);
        NAU_ASSERT(!name.str || !componentName || strcmp(componentName, name.str) == 0, "hash collision <{}> <{}>", name.str, componentName);
#endif
        return componentIndexInEntityArchetype(eid, dataComponents.findComponentId(name.hash));
    }

    inline bool EntityManager::has(EntityId eid, const HashedConstString name) const
    {
        return componentIndexInEntityArchetype(eid, name) != INVALID_ARCHETYPE_COMPONENT_ID;
    }

    inline int EntityManager::getNumComponents(EntityId eid) const
    {
        int idx;
        uint32_t archetype = INVALID_ARCHETYPE;
        if(!getEntityArchetype(eid, idx, archetype))
            return -1;
        return archetypes.getComponentsCount(archetype) - 1;  // first is eid
    }

    inline void* EntityManager::getEntityComponentDataInternal(EntityId eid, uint32_t cid, uint32_t& archetype) const
    {
        int idx;
        if(!getEntityArchetype(eid, idx, archetype) || cid >= archetypes.getComponentsCount(archetype))
            return nullptr;

        // this condition can and should be removed as soon as we have sepatrate callback for constructors
        return archetypes.getComponentDataUnsafe(archetype, cid, archetypes.getComponentSizeUnsafe(archetype, cid), entDescs[idx].chunkId,
                                                 entDescs[idx].idInChunk);
    }

    inline int EntityManager::getArchetypeNumComponents(archetype_t archetype) const
    {
        if(archetype >= archetypes.size())
            return -1;
        return archetypes.getComponentsCount(archetype) - 1;  // first is eid
    }

    inline ecs::component_index_t EntityManager::getArchetypeComponentIndex(archetype_t archetype, uint32_t cid) const
    {
        if(archetype >= archetypes.size())
            return INVALID_COMPONENT_INDEX;

        cid++;  // first is eid
        return archetypes.getComponentUnsafe(archetype, cid);
    }

    inline EntityComponentRef EntityManager::getEntityComponentRef(EntityId eid, uint32_t cid) const
    {
        cid++;  // first is eid
        uint32_t archetype;
        void* data = getEntityComponentDataInternal(eid, cid, archetype);
        if(!data)
            return EntityComponentRef();

        const component_index_t cIndex = archetypes.getComponentUnsafe(archetype, cid);
        ecsdebug::track_ecs_component_by_index_with_stack(cIndex, ecsdebug::TRACK_READ, "getRef", eid);
        DataComponent dataComponentInfo = dataComponents.getComponentById(cIndex);
        return EntityComponentRef(data, dataComponentInfo.componentTypeName, dataComponentInfo.componentType, cIndex);
    }

    inline EntityManager::ComponentInfo EntityManager::getEntityComponentInfo(EntityId eid, uint32_t cid) const
    {
        EntityComponentRef ref = getEntityComponentRef(eid, cid);
        if(ref.isNull())
            return ComponentInfo("<invalid>", eastl::move(ref));
        return ComponentInfo(dataComponents.getComponentNameById(ref.getComponentId()), eastl::move(ref));
    }

    inline const EntityComponentRef EntityManager::getComponentRef(EntityId eid, const HashedConstString name) const
    {
        return getEntityComponentRef(eid, componentIndexInEntityArchetype(eid, name) - 1);  // first is eid, so we don't ask for eid
    }
    inline const EntityComponentRef EntityManager::getComponentRef(EntityId eid, component_index_t cidx) const
    {
        return getEntityComponentRef(eid, componentIndexInEntityArchetype(eid, cidx) - 1);  // first is eid, so we don't ask for eid
    }

    inline EntityComponentRef EntityManager::getComponentRefRW(EntityId eid, component_index_t cidx)
    {
        scheduleTrackChanged(eid, cidx);
        return getEntityComponentRef(eid, componentIndexInEntityArchetype(eid, cidx) - 1);  // first is eid, so we don't ask for eid
    }

    inline bool EntityManager::isEntityComponentSameAsTemplate(ecs::EntityId eid, const EntityComponentRef ref, uint32_t cid) const
    {
        DAECS_EXT_ASSERT(ref.getRawData() != nullptr);
        DAECS_EXT_ASSERT(doesEntityExist(eid));
        cid++;  // first is eid
        EntityDesc entDesc = entDescs[eid.index()];
        const uint32_t ofs = archetypes.getComponentInitialOfs(entDesc.archetype, cid);
        const void* templateData = templates.getTemplateData(entDesc.template_id, ofs, cid);
        if(!templateData)
            return false;
        ComponentType typeInfo = componentTypes.getTypeInfo(ref.getTypeId());
        if(is_pod(typeInfo.flags))
            return memcmp(ref.getRawData(), templateData, typeInfo.size) == 0;
        ComponentTypeManager* ctm = getComponentTypes().getTypeManager(ref.getTypeId());
        return ctm ? ctm->is_equal(ref.getRawData(), templateData) : true;
    }

    inline bool EntityManager::isEntityComponentSameAsTemplate(EntityId eid, uint32_t cid) const
    {
        EntityComponentRef cref = getEntityComponentRef(eid, cid);
        if(!cref.getRawData())
            return false;  // actually error
        return isEntityComponentSameAsTemplate(eid, cref, cid);
    }

    DAECS_RELEASE_INLINE void* __restrict EntityManager::get(EntityId eid, component_index_t index, uint32_t sz, uint32_t& __restrict archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG) const
    {
#if NAU_DEBUG
        if(eid && eid == destroyingEntity && !for_write)
        {  
            // we don't check if we get invalid eid or for write. 'setOptional' to destroying
           // entity is ok, set non optional will cause assert
            logerr("attempt to get (0x{}|{}) from eid = {}|{} during it's destroy", dataComponents.getComponentNameById(index),
                   dataComponents.getComponentTpById(index), entity_id_t(eid), getEntityTemplateName(eid));
        }
#endif
        int idx = eid.index();
        archetype = INVALID_ARCHETYPE;
        if(idx >= entDescs.allocated_size())
            return nullptr;
        const auto entDesc = entDescs[idx];
        if(entDesc.generation != eid.generation())
            return nullptr;
        archetype = entDesc.archetype;
        DAECS_VALIDATE_ARCHETYPE(archetype);
        if(EASTL_UNLIKELY(archetype == INVALID_ARCHETYPE))
            return nullptr;
        archetype_component_id componentInArchetypeIndex = archetypes.getArchetypeComponentIdUnsafe(archetype, index);
        if(componentInArchetypeIndex == INVALID_ARCHETYPE_COMPONENT_ID)
            return nullptr;
#if NAU_DEBUG
        NAU_ASSERT(archetypes.getComponent(entDesc.archetype, componentInArchetypeIndex) == index);
        ecsdebug::track_ecs_component_by_index_with_stack(index, for_write ? ecsdebug::TRACK_WRITE : ecsdebug::TRACK_READ,
                                                          for_write ? "getRW/set" : "get", eid);
#endif
#if NAU_DEBUG
        if(EASTL_UNLIKELY(
               for_write && creatingEntityTop.eid == eid && creatingEntityTop.createdCindex < index &&
               (componentTypes.getTypeInfo(dataComponents.getComponentById(index).componentType).flags & COMPONENT_TYPE_NON_TRIVIAL_CREATE)))
        {
            logerr(
                "attempt to write to component <{}> of type <{}> during creation of <{}>.\n"
                "Consider move writing to ES or put direct dependency (check levelES for example)",
                dataComponents.getComponentNameById(index), componentTypes.getTypeNameById(dataComponents.getComponentById(index).componentType),
                dataComponents.getComponentNameById(creatingEntityTop.createdCindex + 1));
        }
#endif
        return archetypes.getComponentDataUnsafe(archetype, componentInArchetypeIndex, sz, entDesc.chunkId, entDesc.idInChunk);
    }

    inline void* __restrict EntityManager::get(EntityId eid, const HashedConstString name, component_type_t type_name, uint32_t sz, component_index_t& __restrict index, uint32_t& __restrict archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE_ARG) const
    {
        index = dataComponents.findComponentId(name.hash);
        if(index == INVALID_COMPONENT_INDEX)
            return nullptr;
        //-- this check is sanity check. We actually don't do that at all, however, it can happen during development and even in release
        // can be potentially remove from release build, to get extra performance
        DataComponent componentInfo = dataComponents.getComponentById(index);
        if(componentInfo.componentTypeName != type_name)
        {
            logerr("type mismatch on get <{}> <0x{} != requested 0x{}>", dataComponents.getComponentNameById(index),
                    dataComponents.getComponentById(index).componentTypeName, type_name);
            return nullptr;
        }
//-- end of sanity check
#if DAGOR_DBGLEVEL > 1
        NAU_ASSERT(!name.str || strcmp(dataComponents.getComponentNameById(index), name.str) == 0, "hash collision <{}> <{}>", name.str,
                   dataComponents.getComponentNameById(index));
#endif
        return get(eid, index, sz, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE_PASS);
    }

    template <class T>
    DAECS_RELEASE_INLINE T& __restrict EntityManager::getRW(EntityId eid, const HashedConstString name)
    {
        component_index_t cidx;
        uint32_t archetype;
        void* __restrict val =
            get(eid, name, ComponentTypeInfo<T>::type, ComponentTypeInfo<T>::size, cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE);
        if(EASTL_LIKELY(val != nullptr))
        {
            if(ComponentTypeInfo<T>::can_be_tracked)
                scheduleTrackChangedCheck(eid, archetype, cidx);
            return PtrComponentType<T>::ref(val);
        }
        accessError(eid, name);
        return getScratchValue<T>();
    }

    template <class T, typename U>
    DAECS_RELEASE_INLINE U EntityManager::get(EntityId eid, const HashedConstString name) const
    {
        component_index_t cidx;
        uint32_t archetype;
        const void* __restrict val =
            get(eid, name, ComponentTypeInfo<T>::type, ComponentTypeInfo<T>::size, cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        if(EASTL_LIKELY(val != nullptr))
            return PtrComponentType<T>::cref(val);
        accessError(eid, name);
        return getScratchValue<T>();
    }

    template <class T>
    DAECS_RELEASE_INLINE const T* __restrict EntityManager::getNullable(EntityId eid, const HashedConstString name) const
    {
        component_index_t cidx;
        uint32_t archetype;
        const void* __restrict val =
            get(eid, name, ComponentTypeInfo<T>::type, ComponentTypeInfo<T>::size, cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        return (!PtrComponentType<T>::is_boxed || val) ? &PtrComponentType<T>::cref(val) : nullptr;
    }

    template <class T>
    DAECS_RELEASE_INLINE T EntityManager::getOr(EntityId eid, const HashedConstString name, const T& __restrict def) const
    {
        static_assert(!eastl::is_same<T, ecs::string>::value,
                      "Generic \"getOr\" is not for strings, use \"const char* getOr(...)\" method instead");
        component_index_t cidx;
        uint32_t archetype;
        const void* val =
            get(eid, name, ComponentTypeInfo<T>::type, ComponentTypeInfo<T>::size, cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        return val ? PtrComponentType<T>::cref(val) : def;
    }

    // fast functions
    template <class T, typename U>
    DAECS_RELEASE_INLINE U EntityManager::getFast(EntityId eid, const component_index_t cidx, const LTComponentList* list) const
    {
        uint32_t archetype;
        const void* __restrict val = get(eid, cidx, ComponentTypeInfo<T>::size, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        if(EASTL_LIKELY(val != nullptr))
            return PtrComponentType<T>::cref(val);
        accessError(eid, cidx, list);
        return getScratchValue<T>();
    }

    template <class T>
    DAECS_RELEASE_INLINE const T* __restrict EntityManager::getNullableFast(EntityId eid, const component_index_t cidx) const
    {
        uint32_t archetype;
        const void* __restrict val = get(eid, cidx, ComponentTypeInfo<T>::size, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        return (!PtrComponentType<T>::is_boxed || val) ? &PtrComponentType<T>::cref(val) : nullptr;
    }
    template <class T, typename U>
    NAU_FORCE_INLINE U EntityManager::getOrFast(EntityId eid, const component_index_t cidx, const T& __restrict def) const
    {
        static_assert(!eastl::is_same<T, ecs::string>::value, "Generic \"getOrFast\" is not strings, use non-generic method for that");
        uint32_t archetype;
        const void* val = get(eid, cidx, ComponentTypeInfo<T>::size, archetype DAECS_EXTENSIVE_CHECK_FOR_READ);
        return val ? PtrComponentType<T>::cref(val) : def;
    }

    template <class T>
    DAECS_RELEASE_INLINE T& __restrict EntityManager::getRWFast(EntityId eid, const FastGetInfo name, const LTComponentList* __restrict list)
    {
        uint32_t archetype;
        void* __restrict val = get(eid, name.cidx, ComponentTypeInfo<T>::size, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE);
        if(EASTL_LIKELY(val != nullptr))
        {
            if(ComponentTypeInfo<T>::can_be_tracked && name.canBeTracked())
                scheduleTrackChangedCheck(eid, archetype, name.cidx);
            return PtrComponentType<T>::ref(val);
        }
        accessError(eid, name.cidx, list);
        return getScratchValue<T>();
    }

    template <class T>
    DAECS_RELEASE_INLINE T* __restrict EntityManager::getNullableRWFast(EntityId eid, const FastGetInfo name)
    {
        uint32_t archetype;
        void* __restrict val = get(eid, name.cidx, ComponentTypeInfo<T>::size, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE);
        if(!val)
            return nullptr;
        if(ComponentTypeInfo<T>::can_be_tracked && name.canBeTracked())
            scheduleTrackChangedCheck(eid, archetype, name.cidx);
        return &PtrComponentType<T>::ref(val);
    }

    template <class T>
    DAECS_RELEASE_INLINE void EntityManager::setOptionalFast(EntityId eid, const FastGetInfo name, const T& __restrict v)
    {
        T* __restrict to = getNullableRWFast<T>(eid, name);
        if(to)
            *to = v;
    }

    template <class T>
    DAECS_RELEASE_INLINE void EntityManager::setFast(EntityId eid, const FastGetInfo name, const T& __restrict v, const LTComponentList* __restrict list)
    {
        T* __restrict to = getNullableRWFast<T>(eid, name);
        if(to)
            *to = v;
        else
            accessError(eid, name.cidx, list);
    }

    DAECS_RELEASE_INLINE bool EntityManager::archetypeTrackChangedCheck(uint32_t archetypeId, component_index_t cidx)
    {
        if(archetypeId == INVALID_ARCHETYPE)  // do not use archetypes.size(), to avoid memory read
            return false;
        DAECS_VALIDATE_ARCHETYPE(archetypeId);
        const component_index_t old_cidx = dataComponents.getTrackedPair(cidx);
        if(old_cidx == INVALID_COMPONENT_INDEX)
            return false;
        // this is even existence check, we don't care about actual offset.
        // todo: can be optimized with bitvector (we have up to ~2048 total components, so less than 256 bytes)
        // todo: we can check if archetype has tracked at all, even faster
        if(archetypes.getArchetypeComponentIdUnsafe(archetypeId, old_cidx) == INVALID_ARCHETYPE_COMPONENT_ID)
            return false;
        return true;
    }

    inline void EntityManager::scheduleTrackChangedCheck(EntityId eid, uint32_t archetypeId, component_index_t cidx)
    {
        if(archetypeTrackChangedCheck(archetypeId, cidx))
            scheduleTrackChanged(eid, cidx);
    }

    template <class T>
    DAECS_RELEASE_INLINE T* __restrict EntityManager::getNullableRW(EntityId eid, const HashedConstString name)
    {
        component_index_t cidx;
        uint32_t archetype;
        void* __restrict val =
            get(eid, name, ComponentTypeInfo<T>::type, ComponentTypeInfo<T>::size, cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE);
        if(!val)
            return NULL;
        if(ComponentTypeInfo<T>::can_be_tracked)
            scheduleTrackChangedCheck(eid, archetype, cidx);
        return &PtrComponentType<T>::ref(val);
    }

    template <class T, bool optional>
    DAECS_RELEASE_INLINE void EntityManager::setComponentInternal(EntityId eid, const HashedConstString name, const T& __restrict v)
    {
        T* __restrict attr = getNullableRW<T>(eid, name);
        if(attr)
            *attr = v;
        else if(!optional)
            accessError(eid, name);
    }

    template <typename T, bool optional>
    typename eastl::enable_if<eastl::is_rvalue_reference<T&&>::value>::type DAECS_RELEASE_INLINE EntityManager::setComponentInternal(
        EntityId eid,
        const HashedConstString name,
        T&& v)
    {
        T* __restrict attr = getNullableRW<T>(eid, name);
        if(attr)
            *attr = eastl::move(v);
        else if(!optional)
            accessError(eid, name);
    }
   
    inline unsigned get_generation(const EntityId e)
    {
        return e.generation();
    }

    inline bool EntityManager::doesEntityExist(EntityId e) const
    {
        return entDescs.doesEntityExist(e);
    }

    NAU_FORCE_INLINE void EntityManager::sendEventImmediate(EntityId eid, Event&& evt)
    {
        sendEventImmediate(eid, evt);
    }
    NAU_FORCE_INLINE void EntityManager::broadcastEventImmediate(Event&& evt)
    {
        broadcastEventImmediate(evt);
    }

    template <class EventType>
    NAU_FORCE_INLINE void EntityManager::dispatchEvent(EntityId eid, EventType&& evt)  // INVALID_ENTITY_ID means broadcast
    {
        const bool isMtMode = isConstrainedMTMode();
        DAECS_EXT_ASSERT(isMtMode || nau::getApplication().isMainThread());
        DAECS_EXT_ASSERTF(bool(eid) == bool(evt.getFlags() & EVCAST_UNICAST), "event {} has {} flags but sent as {}", evt.getName(),
                          (evt.getFlags() & EVCAST_UNICAST) ? "unicast" : "broadcast", bool(eid) ? "unicast" : "broadcast");
        ScopedMTMutexT<decltype(deferredEventsMutex)> evtMutex(isMtMode, deferredEventsMutex);
        validateEventRegistration(evt, eastl::remove_reference<EventType>::type::staticName());

        deferredEventsCount++;
        eventsStorage.emplaceEvent(eid, eastl::move(evt));
    }

    template <class Storage>
    NAU_FORCE_INLINE void EntityManager::emplaceUntypedEvent(Storage& storage, EntityId eid, Event& evt)
    {
        const uint32_t len = evt.getLength();
        void* __restrict at = storage.allocateUntypedEvent(eid, len);
        if(EASTL_LIKELY(!(evt.getFlags() & EVFLG_DESTROY)))
        {
            memcpy(at, (void*)&evt, len);  // we can memcpy such event
        }
        else
            eventDb.moveOut(at, eastl::move(evt));  // hash lookup
    }

    NAU_FORCE_INLINE void EntityManager::dispatchEvent(EntityId eid, Event& evt)  // INVALID_ENTITY_ID means broadcast
    {
        const bool isMtMode = isConstrainedMTMode();
        
        DAECS_EXT_ASSERT(isMtMode || nau::getApplication().isMainThread());
        DAECS_EXT_ASSERTF(bool(eid) == bool(evt.getFlags() & EVCAST_UNICAST), u8"event {} has {} flags but sent as {}", evt.getName(),
                          (evt.getFlags() & EVCAST_UNICAST) ? "unicast" : "broadcast", bool(eid) ? "unicast" : "broadcast");
        ScopedMTMutexT<decltype(deferredEventsMutex)> evtMutex(isMtMode, deferredEventsMutex);
        validateEventRegistration(evt, nullptr);

        deferredEventsCount++;
        emplaceUntypedEvent(eventsStorage, eid, evt);
    }

    NAU_FORCE_INLINE void EntityManager::dispatchEvent(EntityId eid, Event&& evt)
    {
        dispatchEvent(eid, (Event&)evt);
    }

    template <class EventType>
    inline void EntityManager::sendEvent(EntityId eid, EventType&& evt)
    {
        static_assert(eastl::remove_reference<EventType>::type::staticFlags() & EVCAST_UNICAST);
        if(EASTL_LIKELY(eid))  // attempt to send event to invalid entity is no-op
            dispatchEvent(eid, eastl::move(evt));
    }

    inline void EntityManager::sendEvent(EntityId eid, Event& evt)
    {
        if(EASTL_LIKELY(eid))  // attempt to send event to invalid entity is no-op
            dispatchEvent(eid, evt);
    }

    inline void EntityManager::sendEvent(EntityId eid, Event&& evt)
    {
        sendEvent(eid, (Event&)evt);
    }

    template <class EventType>
    inline void EntityManager::broadcastEvent(EventType&& evt)
    {
        static_assert(eastl::remove_reference<EventType>::type::staticFlags() & EVCAST_BROADCAST);
        dispatchEvent(INVALID_ENTITY_ID, eastl::move(evt));
    }

    inline void EntityManager::broadcastEvent(Event& evt)
    {
        dispatchEvent(INVALID_ENTITY_ID, evt);
    }
    inline void EntityManager::broadcastEvent(Event&& evt)
    {
        broadcastEvent((Event&)evt);
    }

    inline bool EntityManager::destroyEntityAsync(const EntityId& eid)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);  // todo: we can make free threadsafety
        if(!doesEntityExist(eid))
            return false;
        // always postpone destruction
        emplaceDestroy(eid);
        return true;
    }

    inline bool EntityManager::destroyEntityAsync(EntityId& eid)
    {
        bool existed = destroyEntityAsync(static_cast<const EntityId&>(eid));
        eid = INVALID_ENTITY_ID;
        return existed;
    }

    inline void EntityManager::setFilterTags(nau::ConstSpan<const char*> tags)
    {
        templateDB.setFilterTags(tags);
    }

    inline ecs::template_t EntityManager::getEntityTemplateId(ecs::EntityId eid) const
    {
        const unsigned idx = eid.index();
        if(!entDescs.doesEntityExist(idx, eid.generation()))
            return INVALID_TEMPLATE_INDEX;
        return entDescs[idx].template_id;
    }

    inline const char* EntityManager::getEntityTemplateName(ecs::EntityId eid) const
    {
        return getTemplateName(getEntityTemplateId(eid));
    }

    template <bool optional>
    NAU_FORCE_INLINE void EntityManager::setComponentInternal(EntityId eid, const HashedConstString name, ChildComponent&& a)
    {
        component_index_t cidx;
        uint32_t archetype;
        void* data = get(eid, name, a.getUserType(), a.getSize(), cidx, archetype DAECS_EXTENSIVE_CHECK_FOR_WRITE);
        if(!data)
        {
            if(!optional)
                accessError(eid, name);
            return;
        }
        scheduleTrackChangedCheck(eid, archetype, cidx);
        type_index_t componentType = a.componentTypeIndex;
        ComponentType componentTypeInfo = componentTypes.getTypeInfo(componentType);
        if(is_pod(componentTypeInfo.flags))
            memcpy(data, a.getRawData(), a.getSize());
        else
        {
            // NAU_ASSERT(0, "move is not checked");
            ComponentTypeManager* ctm = componentTypes.getTypeManager(componentType);
            if(ctm)
            {
                ctm->assign(data, a.getRawData());
                // ctm->move(data, a.getRawData());a.reset();//should be move, but we don't support move yet
            }
        }
    }

    inline bool EntityManager::isLoadingEntity(EntityId eid) const
    {
        return entDescs.getEntityState(eid) == EntitiesDescriptors::EntityState::Loading;
    }

    inline void EntityManager::set(EntityId eid, const HashedConstString name, ChildComponent&& a)
    {
        setComponentInternal<false>(eid, name, eastl::move(a));
    }

    template <typename T>
    typename eastl::enable_if<eastl::is_rvalue_reference<T&&>::value>::type inline EntityManager::set(EntityId eid,
                                                                                                      const HashedConstString name,
                                                                                                      T&& v)
    {
        setComponentInternal<T, false>(eid, name, eastl::move(v));
    }
    template <class T>
    inline void EntityManager::set(EntityId eid, const HashedConstString name, const T& v)
    {
        setComponentInternal<T, false>(eid, name, v);
    }

    inline void EntityManager::setOptional(EntityId eid, const HashedConstString name, ChildComponent&& a)
    {
        setComponentInternal<true>(eid, name, eastl::move(a));
    }
    template <typename T>
    typename eastl::enable_if<eastl::is_rvalue_reference<T&&>::value>::type inline EntityManager::setOptional(EntityId eid,
                                                                                                              const HashedConstString name,
                                                                                                              T&& v)
    {
        setComponentInternal<T, true>(eid, name, eastl::move(v));
    }
    template <class T>
    inline void EntityManager::setOptional(EntityId eid, const HashedConstString name, const T& v)
    {
        setComponentInternal<T, true>(eid, name, v);
    }

    inline component_index_t EntityManager::createComponent(const HashedConstString name, type_index_t component_type, nau::Span<component_t> non_optional_deps, ComponentSerializer* io, component_flags_t flags)
    {
        ScopedMTMutex lock(isConstrainedMTMode(), creationMutex);
        return dataComponents.createComponent(name, component_type, non_optional_deps, io, flags, componentTypes);
    }

    inline LTComponentList* EntityManager::getComponentLT(const HashedConstString name)
    {
        return dataComponents.componentToLT.at(name.hash);
    }

    inline type_index_t EntityManager::registerType(const HashedConstString name, uint16_t data_size, ComponentSerializer* io, ComponentTypeFlags flags, create_ctm_t ctm, destroy_ctm_t dtm, void* user)
    {
        DAECS_EXT_ASSERT(!isConstrainedMTMode());
        return componentTypes.registerType(name.str, name.hash, data_size, io, flags, ctm, dtm, user);
    }

    inline void EntityManager::flushDeferredEvents()
    {
        sendQueuedEvents(-1);
    }

    inline void EntityManager::setEidsReservationMode(bool on)
    {
        NAU_ASSERT(nextResevedEidIndex <= 1, "{} shall be called before creation of entities with reserved components", __FUNCTION__);
        eidsReservationMode = on;
    }

    struct ComponentsIterator
    {
        typedef EntityManager::ComponentInfo Ret;
        void operator++();
        operator bool() const
        {
            return currentAttr < attrCount;
        }
        Ret operator*() const;

    protected:
        ComponentsIterator() = delete;
        ComponentsIterator(const EntityManager& manager_, EntityId eid_, bool include_, int attr_count);
        friend class EntityManager;
        const EntityManager& manager;
        EntityId eid = INVALID_ENTITY_ID;
        int currentAttr = 0, attrCount = 0;
        bool including_templates = false;
    };

    inline ComponentsIterator EntityManager::getComponentsIterator(EntityId eid, bool including_templates) const
    {
        return ComponentsIterator(*this, eid, including_templates, getNumComponents(eid));
    }

    inline ComponentsIterator::ComponentsIterator(const EntityManager& manager_, EntityId eid_, bool include_, int attr_count) :
        manager(manager_),
        eid(eid_),
        including_templates(include_),
        attrCount(attr_count)
    {
    }

    inline EntityManager::ComponentInfo ComponentsIterator::operator*() const
    {
        return manager.getEntityComponentInfo(eid, currentAttr);
    }

    inline void ComponentsIterator::operator++()
    {
        currentAttr++;
        if(!including_templates)
            for(; currentAttr < attrCount && manager.isEntityComponentSameAsTemplate(eid, currentAttr);)
                ++currentAttr;
    }

    //inline uint32_t EntityManager::getMaxWorkerId() const TODO Uncomment or delete
    //{
    //    return maxNumJobs;
    //}

    template <typename Fn>
    bool EntityManager::performEidQuery(ecs::EntityId eid, QueryId h, Fn&& fun, void* user_data)
    {
        uint32_t eidIdx = eid.index();
        if(eidIdx >= entDescs.allocated_size())
            return false;
        const auto& entDesc = entDescs[eidIdx];
        archetype_t archetype = entDesc.archetype;
        if(entDesc.generation != eid.generation() || archetype == INVALID_ARCHETYPE)
            return false;

        QueryView qv(*this, user_data);
        QueryView::ComponentsData componentData[MAX_ONE_EID_QUERY_COMPONENTS];
        qv.componentData = componentData;
        if(!fillEidQueryView(eid, entDesc, h, qv))
            return false;

        ecsdebug::track_ecs_component(queryDescs[h.index()].getDesc(), queryDescs[h.index()].getName(), eid);

        const bool isConstrained = isConstrainedMTMode();
        if(!isConstrained)
            nestedQuery++;
        fun(qv);
        if(!isConstrained)
            nestedQuery--;
        return true;
    }

    template <class T>
    inline const T* EntityManager::getRequestingBase(const HashedConstString name) const
    {
        const CurrentlyRequesting& creating = *requestingTop;
        // initializer
        for(auto& init : creating.initializer)  // linear search!
            if(init.name == name.hash)
                return init.second.getNullable<T>();

        // check current entity data (from archetype eventsStorage)
        if(EASTL_UNLIKELY(creating.oldArchetype != INVALID_ARCHETYPE))
        {
            const T* old = getNullable<T>(creating.eid, name);
            if(old)
                return old;
        }

        // new template
        const archetype_t newArchetype = creating.newArchetype;
        const template_t newTemplate = creating.newTemplate;
        const component_index_t index = dataComponents.findComponentId(name.hash);
        if(index == INVALID_COMPONENT_INDEX)
            return nullptr;

        //-- this check is sanity check. We actually don't do that at all, however, it can happen during development and even in release
        // can be potentially remove from release build, to get extra performance
        DataComponent componentInfo = dataComponents.getComponentById(index);
        const component_type_t type_name = ComponentTypeInfo<T>::type;
        if(componentInfo.componentTypeName != type_name)
        {
            logwarn("type mismatch on get <{}> <0x{} != requested 0x{}>", dataComponents.getComponentNameById(index),
                    componentInfo.componentTypeName, type_name);
            return nullptr;
        }

        archetype_component_id componentInArchetypeIndex = archetypes.getArchetypeComponentId(newArchetype, index);
        if(componentInArchetypeIndex == INVALID_ARCHETYPE_COMPONENT_ID)
            return nullptr;

        const uint8_t* __restrict templateData = templates.getTemplate(newTemplate).initialData.get();
        return &PtrComponentType<T>::cref(templateData + archetypes.initialComponentDataOffset(newArchetype)[componentInArchetypeIndex]);
    }

    template <class T>
    inline const T& EntityManager::getRequesting(const HashedConstString name) const
    {
        const T* comp = getRequestingBase<T>(name);
        if(EASTL_LIKELY(comp != nullptr))
            return *comp;
        accessError(requestingTop->eid, name);
        return getScratchValue<T>();
    }

    template <class T>
    inline const T& EntityManager::getRequesting(const HashedConstString name, const T& def) const
    {
        const T* comp = getRequestingBase<T>(name);
        return comp == nullptr ? def : *comp;
    };

    class ResourceRequestCb
    {
    public:
        template <class T>
        const T& get(const HashedConstString hashed_name) const
        {
            return mgr.getRequesting<T>(hashed_name);
        }
        template <class T>
        const T& getOr(const HashedConstString hashed_name, const T& def) const
        {
            return mgr.getRequesting<T>(hashed_name, def);
        }
        template <class T>
        const T* getNullable(const HashedConstString hashed_name) const
        {
            return mgr.getRequestingBase<T>(hashed_name);
        }
        void operator()(const char* n, uint32_t type_id) const
        {
            NAU_ASSERT_RETURN(n && *n, );
            requestedResources.emplace(n, type_id);
        }
        EA_NON_COPYABLE(ResourceRequestCb)
    protected:
        ResourceRequestCb(const EntityManager& m, EntityId eid) :
            mgr(m), eid(eid)
        {
        }
        const EntityManager& mgr;
        mutable gameres_list_t requestedResources;
        friend class EntityManager;
    public:
        const EntityId eid;
    };

    inline void EntityManager::setReplicationCb(replication_cb_t cb)
    {
        replicationCb = cb;
    }

};  // namespace ecs
