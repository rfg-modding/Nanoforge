using System.Collections;
using Common;
using System;

namespace Nanoforge.App
{
    //Can submit or receive events. Events are just structs
    public static class Events
    {
        private static Dictionary<Type, EventData> _eventData = new .() ~DeleteDictionaryAndValues!(_);

        //Called at the end of each frame. Runs all event instances from this frame through each event callback.
        public static void EndFrame()
        {
            for (var data in _eventData)
                data.value.UpdateListeners();
        }

        //Clear all event data and callbacks
        public static void Reset()
        {
            ClearDictionaryAndDeleteValues!(_eventData);
        }

        //Send event to listeners
        public static void Send<T>(T event) where T : struct
        {
            var data = GetOrCreateEventData<T>();
            var instances = data.Instances<T>();
            instances.Add(event);
        }

        //Register delegate that will be called every time a T event is sent
        public static void Listen<T>(delegate void(ref T event) eventHandler) where T : struct
        {
            var data = GetOrCreateEventData<T>();
            var listeners = data.Listeners<T>();
            listeners.Add(eventHandler);
        }

        //Create new EventData instance
        private static EventData CreateEventData<T>() where T : struct
        {
            var instances = new List<T>();
            var listeners = new List<delegate void(ref T event)>();
            delegate void() updateListeners = new [?] () =>
                {
                    for (var instance in ref instances)
                        for (var listener in listeners)
                            listener(ref instance);

                    instances.Clear();
                };
            delegate void() destroy = new [?] () =>
                {
                    for (var listener in listeners)
                        delete listener;

                    instances.Clear();
                };

            return new EventData(typeof(T), instances, listeners, updateListeners, destroy);
        }

        //Get and return event data for type T. Create it if it doesn't exist.
        private static EventData GetOrCreateEventData<T>() where T : struct
        {
            var type = typeof(T);
            if (_eventData.ContainsKey(type))
                return _eventData[type];
            else
                return _eventData.Add(type, .. CreateEventData<T>());
        }

        //Data for a single event
        private class EventData
        {
            public this(Type eventType, Object instances, Object listeners, delegate void() updateListenersDelegate, delegate void() destroyDelegate)
            {
                _eventType = eventType;
                _instances = instances;
                _listeners = listeners;
                _updateListenersDelegate = updateListenersDelegate;
                _destroyDelegate = destroyDelegate;
            }

            public ~this()
            {
                Destroy();
                delete _instances;
                delete _listeners;
                delete _updateListenersDelegate;
                delete _destroyDelegate;
            }

            //Event data type
            private Type _eventType;
            public Type EventType => _eventType;

            //Event instances submitted this frame
            private Object _instances;
            public List<T> Instances<T>() => (List<T>)_instances;

            //Listeners that are triggered for each instance
            private Object _listeners;
            public List<delegate void(ref T event)> Listeners<T>() => (List<delegate void(ref T event)>)_listeners;

            //Used to update listeners in non-generic code
            private delegate void() _updateListenersDelegate;
            public void UpdateListeners()
            {
                _updateListenersDelegate();
            }

            //Used to destruct generic lists in non-generic code
            private delegate void() _destroyDelegate;
            public void Destroy()
            {
                _destroyDelegate();
            }
        }
    }
}