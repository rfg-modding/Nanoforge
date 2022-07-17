using System;

namespace Nanoforge.App
{
	//Interface exposed by all Resource<T> instances
	public interface IResource
	{
		//Get type of data being stored in the resource
		public Type GetValueType();

		//Get pointer to data being stored in the resource
		public Object Get();
	}

	//Global data with a single instance
	public class Resource<T> : IResource where T : class, delete
	{
		private T _value ~if (!_isSystem) delete _; //Don't auto delete systems, App has full ownership of these
        private bool _isSystem;

		public this(T value, bool isSystem = false)
		{
			_value = value;
            _isSystem = isSystem;
		}

		//TODO: Make separate mutable and immutable reference types that can be requested in system args like what Bevy does
		public void Set(T newValue) => _value = newValue;
		public ref T GetMutable() => ref _value;
		public T GetImmutable() => _value;

		//Get type of _value
		Type IResource.GetValueType()
		{
			return typeof(T);
		}

		//Get pointer to _value
		Object IResource.Get()
		{
			return _value;
		}
	}
}
