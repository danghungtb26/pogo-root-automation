public class TapGesture : Gesture // TypeDefIndex: 41217
{
	// Fields
	public const string TAP_MESSAGE = "OnTap";
	private EventHandler<EventArgs> tappedInvoker; // 0xE0
	[SerializeField]
	private int numberOfTapsRequired; // 0xE8
	[SerializeField]
	[NullToggle(NullFloatValue = ∞)]
	private float timeLimit; // 0xEC
	[SerializeField]
	[NullToggle(NullFloatValue = ∞)]
	private float distanceLimit; // 0xF0
	private float distanceLimitInPixelsSquared; // 0xF4
	private bool isActive; // 0xF8
	private int tapsDone; // 0xFC
	private Vector2 startPosition; // 0x100
	private Vector2 totalMovement; // 0x108
	private Coroutine waitRoutine; // 0x110

	// Properties
	public int NumberOfTapsRequired { get; set; }
	public float TimeLimit { get; set; }
	public float DistanceLimit { get; set; }

	// Methods

	// RVA: 0xA2C7CE4 Offset: 0xA2C3CE4 VA: 0xA2C7CE4
	public void add_Tapped(EventHandler<EventArgs> value) { }

	// RVA: 0xA2C7D9C Offset: 0xA2C3D9C VA: 0xA2C7D9C
	public void remove_Tapped(EventHandler<EventArgs> value) { }

	// RVA: 0xA2C7E54 Offset: 0xA2C3E54 VA: 0xA2C7E54
	public int get_NumberOfTapsRequired() { }

	// RVA: 0xA2C7E5C Offset: 0xA2C3E5C VA: 0xA2C7E5C
	public void set_NumberOfTapsRequired(int value) { }

	// RVA: 0xA2C7E6C Offset: 0xA2C3E6C VA: 0xA2C7E6C
	public float get_TimeLimit() { }

	// RVA: 0xA2C7E74 Offset: 0xA2C3E74 VA: 0xA2C7E74
	public void set_TimeLimit(float value) { }

	// RVA: 0xA2C7E7C Offset: 0xA2C3E7C VA: 0xA2C7E7C
	public float get_DistanceLimit() { }

	// RVA: 0xA2C7E84 Offset: 0xA2C3E84 VA: 0xA2C7E84
	public void set_DistanceLimit(float value) { }

	// RVA: 0xA2C7F44 Offset: 0xA2C3F44 VA: 0xA2C7F44 Slot: 15
	protected override void OnEnable() { }

	// RVA: 0xA2C8004 Offset: 0xA2C4004 VA: 0xA2C8004 Slot: 20
	protected override void touchBegan(TouchPoint touch) { }

	// RVA: 0xA2C8204 Offset: 0xA2C4204 VA: 0xA2C8204 Slot: 21
	protected override void touchMoved(TouchPoint touch) { }

	// RVA: 0xA2C8260 Offset: 0xA2C4260 VA: 0xA2C8260 Slot: 22
	protected override void touchEnded(TouchPoint touch) { }

	// RVA: 0xA2C83A4 Offset: 0xA2C43A4 VA: 0xA2C83A4 Slot: 28
	protected override void onRecognized() { }

	// RVA: 0xA2C8450 Offset: 0xA2C4450 VA: 0xA2C8450 Slot: 24
	protected override void reset() { }

	// RVA: 0xA2C8528 Offset: 0xA2C4528 VA: 0xA2C8528 Slot: 19
	protected override bool shouldCacheTouchPosition(TouchPoint value) { }

	[IteratorStateMachine(typeof(TapGesture.<wait>d__30))]
	// RVA: 0xA2C81A8 Offset: 0xA2C41A8 VA: 0xA2C81A8
	private IEnumerator wait() { }

	// RVA: 0xA2C8580 Offset: 0xA2C4580 VA: 0xA2C8580
	public void .ctor() { }
}

