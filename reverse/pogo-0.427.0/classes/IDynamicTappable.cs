public interface IDynamicTappable // TypeDefIndex: 4166
{
	// Properties
	public abstract Vector3 Position { get; }
	public abstract int Id { get; }
	public abstract ulong CellId { get; }
	public abstract ulong EncounterId { get; }
	[Nullable(0)]
	public abstract string TypeKey { get; }
	public abstract LatLng Location { get; }
	public abstract TappableLocation TappableLocation { get; }
	public abstract bool VisibleOnMap { get; set; }
	public abstract long DespawnTimeMs { get; }
	public abstract bool IsEncounter { set; }

	// Methods

	// RVA: -1 Offset: -1 Slot: 0
	public abstract Vector3 get_Position();

	// RVA: -1 Offset: -1 Slot: 1
	public abstract int get_Id();

	// RVA: -1 Offset: -1 Slot: 2
	public abstract ulong get_CellId();

	// RVA: -1 Offset: -1 Slot: 3
	public abstract ulong get_EncounterId();

	[NullableContext(0)]
	// RVA: -1 Offset: -1 Slot: 4
	public abstract string get_TypeKey();

	// RVA: -1 Offset: -1 Slot: 5
	public abstract LatLng get_Location();

	// RVA: -1 Offset: -1 Slot: 6
	public abstract TappableLocation get_TappableLocation();

	// RVA: -1 Offset: -1 Slot: 7
	public abstract bool get_VisibleOnMap();

	// RVA: -1 Offset: -1 Slot: 8
	public abstract void set_VisibleOnMap(bool value);

	// RVA: -1 Offset: -1 Slot: 9
	public abstract long get_DespawnTimeMs();

	// RVA: -1 Offset: -1 Slot: 10
	public abstract void set_IsEncounter(bool value);

	// RVA: -1 Offset: -1 Slot: 11
	public abstract void AnimateInteraction(Action onAnimationEnd);
}

