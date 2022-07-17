using Nanoforge;
using System;

namespace Nanoforge.Misc.Containers
{
	//Essentially a typeless version of SparseList<T>. Used in situations where the stored type is only known at runtime. E.g. ECS component serialization.
	//Using SparseList<T> is preferred when possible since the stored type is more clear.
	public class SparsePool : SparseSet
	{
		//Element type
		private Type _type;
		private u32 _elementSize = 0;
		private u32 _numElements = 0;
		private u32 _denseCapacity = 256;
		//Element buffer
		private u8[] _storage = null;
		//Override of SparseSet DensePtr
		public override void* GetDensePtr() { return &_storage[0]; }
		//Get last element from dense array
		public void* Back
		{
			get
			{
				if (_numElements == 0)
					return &_storage[0];
				else
					return &_storage[_elementSize * (_numElements - 1)];
			}
		};
		public ref T Back<T>() => ref *(T*)Back;

		//Get pointer to the end of the last element
		private void* End => &_storage[_elementSize * _numElements];

		public this(Type type, u32 capacity = 65536) : base(capacity)
		{
			_type = type;
			_elementSize = (u32)type.Stride;
			_storage = new u8[_elementSize * _denseCapacity];
		}

		public ~this()
		{
			delete _storage;
		}

		//Returns true if the index has a value associated with it
		public new bool Contains(u32 index)
		{
			return base.Contains(index);
		}

		//Sets the value at the provided index. Undefined behavior is the index is already in the set.
		public void Add<T>(u32 index, T item)
		{
			if (_numElements == _denseCapacity)
				Resize();

			*(T*)End = item; //Append to back of buffer
			base.Add(index);
			_numElements++;
		}

		//Return reference to the value associated with the provided index
		public ref T Get<T>(u32 index)
		{
			return ref *(T*)&_storage[base.Index(index) * _elementSize];
		}

		//Remove value at provided index
		public new void Remove(u32 index)
		{
			//Move target to back of array and pop back. base.Remove updates the underlying sparse set.
			Internal.MemCpy(Back, &_storage[base.Index(index) * _elementSize], _elementSize);
			_numElements--; //Remove back element
			base.Remove(index);
		}

		//Clear all values
		public new void Clear()
		{
			base.Clear();
			_numElements = 0;
		}

		//Resize internal buffer. If passed 0 it will just double the current capacity
		public void Resize(u32 newCapacity = 0)
		{
			//Double current capacity if passed 0
			u32 newDenseCapacity = newCapacity;
			if (newDenseCapacity == 0)
				newDenseCapacity = _denseCapacity * 2;

			//Create new buffer and copy existing data to it
			u8[] newBuffer = new u8[_elementSize * newDenseCapacity];
			Internal.MemCpy(&newBuffer[0], &_storage[0], _elementSize * _denseCapacity);

			//Delete old buffer
			delete _storage;
			_storage = newBuffer;
			_denseCapacity = newDenseCapacity;
		}
	}
}