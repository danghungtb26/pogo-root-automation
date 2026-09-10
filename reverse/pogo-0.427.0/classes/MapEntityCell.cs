public class MapEntityCell : MonoBehaviour, IMapEntityCell, IScope, IDisposable, IInitializer<ulong, Transform> // TypeDefIndex: 14425
{
	// Fields
	[Inject]
	private readonly IMapEntityService eghs; // 0x20
	private readonly Dictionary<string, IMapPlace> eght; // 0x28
	private readonly Dictionary<string, IMapStation> eghu; // 0x30
	private readonly Dictionary<int, IMapSocialExperiment> eghv; // 0x38
	private readonly Dictionary<ulong, IWildMapPokemon> eghw; // 0x40
	private readonly Dictionary<ulong, IDynamicTappable> eghx; // 0x48
	[CompilerGenerated]
	private ulong <Id>k__BackingField; // 0x50
	[CompilerGenerated]
	private Action OutOfScope; // 0x58
	[CompilerGenerated]
	private readonly List<NearbyPokemonProto> <NearbyPokemon>k__BackingField; // 0x60

	// Properties
	public ulong Id { get; set; }
	public Transform Transform { get; }
	public List<NearbyPokemonProto> NearbyPokemon { get; }
	public IEnumerable<IWildMapPokemon> AllCellWildPokemon { get; }
	public IEnumerable<IDynamicTappable> AllCellWildTappable { get; }
	public IEnumerable<IMapPlace> MapPlacesList { get; }

	// Methods

	[CompilerGenerated]
	// RVA: 0x7F5FA1C Offset: 0x7F5BA1C VA: 0x7F5FA1C Slot: 9
	public ulong get_Id() { }

	[CompilerGenerated]
	// RVA: 0x7F5FA24 Offset: 0x7F5BA24 VA: 0x7F5FA24
	private void set_Id(ulong value) { }

	// RVA: 0x7F5FA2C Offset: 0x7F5BA2C VA: 0x7F5FA2C Slot: 6
	public Transform get_Transform() { }

	[CompilerGenerated]
	// RVA: 0x7F5FA34 Offset: 0x7F5BA34 VA: 0x7F5FA34 Slot: 26
	public void add_OutOfScope(Action value) { }

	[CompilerGenerated]
	// RVA: 0x7F5FAD0 Offset: 0x7F5BAD0 VA: 0x7F5FAD0 Slot: 27
	public void remove_OutOfScope(Action value) { }

	[CompilerGenerated]
	// RVA: 0x7F5FB6C Offset: 0x7F5BB6C VA: 0x7F5FB6C Slot: 5
	public List<NearbyPokemonProto> get_NearbyPokemon() { }

	// RVA: 0x7F5FB74 Offset: 0x7F5BB74 VA: 0x7F5FB74 Slot: 7
	public IEnumerable<IWildMapPokemon> get_AllCellWildPokemon() { }

	// RVA: 0x7F5FBC0 Offset: 0x7F5BBC0 VA: 0x7F5FBC0 Slot: 8
	public IEnumerable<IDynamicTappable> get_AllCellWildTappable() { }

	// RVA: 0x7F5FC0C Offset: 0x7F5BC0C VA: 0x7F5FC0C Slot: 4
	public IEnumerable<IMapPlace> get_MapPlacesList() { }

	// RVA: 0x7F5FCE8 Offset: 0x7F5BCE8 VA: 0x7F5FCE8 Slot: 29
	public void Initialize(ulong cellId, Transform parent) { }

	// RVA: 0x7F5FDA8 Offset: 0x7F5BDA8 VA: 0x7F5FDA8 Slot: 20
	public void RemoveMapTappable(ulong id) { }

	// RVA: 0x7F5FDFC Offset: 0x7F5BDFC VA: 0x7F5FDFC Slot: 21
	public bool TryGetMapPlace(string mapPlaceId, out IMapPlace place) { }

	// RVA: 0x7F5FE60 Offset: 0x7F5BE60 VA: 0x7F5FE60 Slot: 22
	public bool RemoveMapPlace(string mapPlaceId, out IMapPlace place) { }

	// RVA: 0x7F5FEC4 Offset: 0x7F5BEC4 VA: 0x7F5FEC4 Slot: 23
	public bool TryGetMapStation(string mapStationId, out IMapStation station) { }

	// RVA: 0x7F5FF28 Offset: 0x7F5BF28 VA: 0x7F5FF28 Slot: 24
	public void RemoveAndShutdownStations(List<string> stationIds) { }

	// RVA: 0x7F60100 Offset: 0x7F5C100 VA: 0x7F60100 Slot: 25
	public void UnregisterStation(string stationId) { }

	// RVA: 0x7F60154 Offset: 0x7F5C154 VA: 0x7F60154 Slot: 10
	public void AddMapPlace(IMapPlace place) { }

	// RVA: 0x7F60304 Offset: 0x7F5C304 VA: 0x7F60304 Slot: 11
	public void AddMapStation(IMapStation station) { }

	// RVA: 0x7F6043C Offset: 0x7F5C43C VA: 0x7F6043C Slot: 14
	public void AddMapPokemon(IWildMapPokemon pokemon) { }

	// RVA: 0x7F60578 Offset: 0x7F5C578 VA: 0x7F60578 Slot: 15
	public IDynamicTappable GetMapTappable(ulong tappableId) { }

	// RVA: 0x7F605E4 Offset: 0x7F5C5E4 VA: 0x7F605E4 Slot: 16
	public void AddMapTappable(IDynamicTappable tappable) { }

	// RVA: 0x7F60720 Offset: 0x7F5C720 VA: 0x7F60720 Slot: 13
	public IWildMapPokemon GetMapPokemon(ulong encounterId) { }

	// RVA: 0x7F6078C Offset: 0x7F5C78C VA: 0x7F6078C Slot: 19
	public void RemoveMapPokemon(ulong encounterId) { }

	// RVA: 0x7F6086C Offset: 0x7F5C86C VA: 0x7F6086C Slot: 12
	public void UpdateNearbyPokemon(IEnumerable<NearbyPokemonProto> updatedPokemon, ref List<ulong> removedPokemonIds) { }

	// RVA: 0x7F60AA0 Offset: 0x7F5CAA0 VA: 0x7F60AA0 Slot: 17
	public IMapSocialExperiment GetSocialExperiment(int experimentId) { }

	// RVA: 0x7F60B0C Offset: 0x7F5CB0C VA: 0x7F60B0C Slot: 18
	public void AddSocialExperiment(IMapSocialExperiment socialExperiment) { }

	// RVA: 0x7F60D54 Offset: 0x7F5CD54 VA: 0x7F60D54 Slot: 28
	public void Dispose() { }

	// RVA: 0x7F613B0 Offset: 0x7F5D3B0 VA: 0x7F613B0
	public void .ctor() { }
}

