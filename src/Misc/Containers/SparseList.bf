using System.Collections;
using Common;
using System;

namespace Nanoforge.Misc.Containers
{
	//Densely packed array with sparse indices.
	//E.g. You might only have values at indices 0, 10, and 17, and though there are indices between the values are still in a dense contiguous array
	public class SparseList<T> : SparseSet, IEnumerable<T>
	{
		private append List<T> _storage;
		//Get ptr to the dense array of T
		public T* Ptr { get { return _storage.Ptr; } }
		//Get last element from dense array
		public ref T Back => ref _storage.Back;

        //Index operators
        public ref T this[u32 index] => ref _storage[base.Index(index)];

		public this(u32 capacity = 65536) : base(capacity)
		{

		}

		//Returns true if the index has a value associated with it
		public new bool Contains(u32 index)
		{
			return base.Contains(index);
		}

		//Sets the value at the provided index. Undefined behavior is the index is already in the set.
		public void Add(u32 index, T item)
		{
			_storage.Add(item);
			base.Add(index);
		}

		//Remove value at provided index
		public new void Remove(u32 index)
		{
			//Move target to back of array and pop back. base.Remove updates the underlying sparse set.
			Swap!(_storage.Back, _storage[base.Index(index)]);
			_storage.PopBack();
			base.Remove(index);
		}

		//Clear all values
		public new void Clear()
		{
			base.Clear();
			_storage.Clear();
		}

        public List<T>.Enumerator GetEnumerator()
        {
            return _storage.GetEnumerator();
        }
	}
}