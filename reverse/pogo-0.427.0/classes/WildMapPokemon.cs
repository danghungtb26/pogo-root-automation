public class WildMapPokemon : MapPokemon, IWildMapPokemon, IMapPokemon, IScope, IDisposable, IInitializer<WildPokemonProto>, IInitializer<WildMapPokemon.Data> // TypeDefIndex: 14536
{
	// Fields
	[SerializeField]
	private GameObject normalRippleFx; // 0x218
	[SerializeField]
	private GameObject weatherRippleFx; // 0x220
	[Inject]
	private readonly ILocationProvider egwm; // 0x228
	[Inject]
	private readonly IRpcHandler egwn; // 0x230
	[Inject]
	private readonly IToastService egwo; // 0x238
	[Inject]
	private readonly IMapEntityService egwp; // 0x240
	[Inject]
	private readonly LazyInject<IEncounterState> egwq; // 0x248
	private WildPokemonProto egwr; // 0x250
	private EncounterOutProto egws; // 0x258
	private PokemonProto egwt; // 0x260
	private IDynamicTappable egwu; // 0x268
	private TappableEncounterProto egwv; // 0x270
	private bool egww; // 0x278

	// Properties
	private IEncounterState escb { get; }
	private bool escc { get; }
	public override ulong EncounterId { get; }
	public override PokemonProto Pokemon { get; }
	public override PokemonDisplayProto PokemonDisplay { get; }
	public override int PokemonId { get; }
	public override string SpawnPointId { get; }
	public override int Cp { get; }
	public override CharacterSize Size { get; }
	public override int AppraisalStar { get; }
	public bool VisibleOnMap { get; set; }

	// Methods

	// RVA: 0x7F9715C Offset: 0x7F9315C VA: 0x7F9715C
	private IEncounterState ckcs() { }

	// RVA: 0x7F971A8 Offset: 0x7F931A8 VA: 0x7F971A8
	private bool ckct() { }

	// RVA: 0x7F971B8 Offset: 0x7F931B8 VA: 0x7F971B8 Slot: 29
	public override ulong get_EncounterId() { }

	// RVA: 0x7F972BC Offset: 0x7F932BC VA: 0x7F972BC Slot: 31
	public override PokemonProto get_Pokemon() { }

	// RVA: 0x7F972C4 Offset: 0x7F932C4 VA: 0x7F972C4 Slot: 32
	public override PokemonDisplayProto get_PokemonDisplay() { }

	// RVA: 0x7F972DC Offset: 0x7F932DC VA: 0x7F972DC Slot: 33
	public override int get_PokemonId() { }

	// RVA: 0x7F972F4 Offset: 0x7F932F4 VA: 0x7F972F4 Slot: 30
	public override string get_SpawnPointId() { }

	// RVA: 0x7F974F0 Offset: 0x7F934F0 VA: 0x7F974F0 Slot: 34
	public override int get_Cp() { }

	// RVA: 0x7F975A0 Offset: 0x7F935A0 VA: 0x7F975A0 Slot: 37
	public override CharacterSize get_Size() { }

	// RVA: 0x7F9766C Offset: 0x7F9366C VA: 0x7F9766C Slot: 38
	public override int get_AppraisalStar() { }

	// RVA: 0x7F976EC Offset: 0x7F936EC VA: 0x7F976EC Slot: 47
	public bool get_VisibleOnMap() { }

	// RVA: 0x7F976F4 Offset: 0x7F936F4 VA: 0x7F976F4 Slot: 48
	public void set_VisibleOnMap(bool value) { }

	// RVA: 0x7F977F0 Offset: 0x7F937F0 VA: 0x7F977F0
	private void ckcu(bool a) { }

	// RVA: 0x7F97C20 Offset: 0x7F93C20 VA: 0x7F97C20 Slot: 50
	public void Initialize(WildPokemonProto proto) { }

	// RVA: 0x7F980F8 Offset: 0x7F940F8 VA: 0x7F980F8 Slot: 51
	public void Initialize(WildMapPokemon.Data data) { }

	// RVA: 0x7F98004 Offset: 0x7F94004 VA: 0x7F98004
	private void ckcv() { }

	// RVA: 0x7F98564 Offset: 0x7F94564 VA: 0x7F98564 Slot: 41
	public override IPromise<PokemonEncounterResponse> SendEncounterRequest() { }

	// RVA: 0x7F98CE4 Offset: 0x7F94CE4 VA: 0x7F98CE4
	private bool ckcw(EncounterOutProto a, out bool b, out bool c) { }

	// RVA: 0x7F990F0 Offset: 0x7F950F0 VA: 0x7F990F0 Slot: 45
	protected override void HandleDestroy() { }

	// RVA: 0x7F97710 Offset: 0x7F93710 VA: 0x7F97710
	private void ckcx() { }

	// RVA: 0x7F991B4 Offset: 0x7F951B4 VA: 0x7F991B4 Slot: 49
	public void Dispose() { }

	// RVA: 0x7F991C4 Offset: 0x7F951C4 VA: 0x7F991C4
	public void .ctor() { }

	// RVA: 0x7F9925C Offset: 0x7F9525C VA: 0x7F9925C Slot: 7
	private Transform Niantic.Holoholo.Map.IMapPokemon.get_transform() { }

	[CompilerGenerated]
	// RVA: 0x7F99264 Offset: 0x7F95264 VA: 0x7F99264
	private void ckcy() { }
}

