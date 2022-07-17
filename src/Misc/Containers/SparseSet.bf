using System.Collections;
using Nanoforge;
using System;

namespace Nanoforge.Misc.Containers
{
	//Maps indices in a sparse array to indices in a dense array
	//O(1) adding/removal of elements, checking for index existence, and quick iteration since the IDs are stored in a contiguous array.
	//Based on EnTT and this article from it's author: https://skypjack.github.io/2020-08-02-ecs-baf-part-9/
	public class SparseSet
	{
		private append List<u32> _sparse;
		private append List<u32> _dense;
		private u32 _emptyIndexValue;

		//Number of elements in set
		public int Count => _dense.Count;
		//Pointer to dense index array. If the concrete type is a SparseList<T> this will instead return a pointer to the dense element array.
		public virtual void* GetDensePtr() { return _dense.Ptr; }

		public this(u32 capacity = 65536)
		{
			//Create sparse set and set all values to _emptyIndexValue
			_emptyIndexValue = capacity;
			_sparse.Reserve(capacity);
			for (u32 i = 0; i < capacity; i++)
				_sparse.Add(_emptyIndexValue);
		}

		//Returns true if the index has data associated with it
		public bool Contains(u32 index)
		{
			return index < _sparse.Count && _sparse[index] !=  _emptyIndexValue;
		}

		//Adds the index to the set. Undefined behavior if the index is already in the set.
		public void Add(u32 index)
		{
#if DEBUG
			if (Contains(index))
				Runtime.FatalError("Tried to set index of a SparseSet that already has a value!");
#endif
			_sparse[index] = (u32)_dense.Count;
			_dense.Add(index);
		}

		//Remove value at provided index from set
		public void Remove(u32 index)
		{
			if (index >= _sparse.Count)
				return;

			u32 last = _dense.Back;
			Swap!(_dense.Back, _dense[_sparse[index]]); //Swap target value with back and pop
			Swap!(_sparse[last], _sparse[index]); //Update index of back value that got swapped with target
			_sparse[index] = _emptyIndexValue; //Set target index to empty
			_dense.PopBack();
		}

		//Get the dense array index associated with the provided sparse index
		public u32 Index(u32 index)
		{
			return _sparse[index];
		}

		//Clear all indices
		public void Clear()
		{
			//Clear dense array and set all values in sparse array to empty
			_dense.Clear();
			for (u32 i = 0; i < _sparse.Count; i++)
				_sparse[i] = _emptyIndexValue;
		}
	}
}