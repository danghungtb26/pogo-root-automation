package dev.pogoroot.automation.location

import android.content.Context
import android.location.Criteria
import android.location.Location
import android.location.LocationManager
import android.os.SystemClock
import dev.pogoroot.automation.core.model.GeoPoint
import dev.pogoroot.automation.root.ProcessRootShell
import dev.pogoroot.automation.root.RootShell

interface MockLocationSink {
    fun start(): Result<Unit>
    fun publish(point: GeoPoint, speedMetersPerSecond: Float, bearingDegrees: Float): Result<Unit>
    fun stop()
}

class RootMockLocationProvider(
    context: Context,
    private val rootShell: RootShell = ProcessRootShell(),
) : MockLocationSink {
    private val appContext = context.applicationContext
    private val locationManager = appContext.getSystemService(LocationManager::class.java)
    private var started = false

    override fun start(): Result<Unit> = runCatching {
        val appOps = rootShell.execute(
            "appops set ${appContext.packageName} android:mock_location allow",
            timeoutMillis = 5_000L,
        )
        check(appOps.isSuccess) {
            "Unable to grant mock-location app-op via root: ${appOps.stderr.ifBlank { appOps.stdout }}"
        }

        configureProvider(
            name = LocationManager.GPS_PROVIDER,
            requiresNetwork = false,
            requiresSatellite = true,
            requiresCell = false,
            powerRequirement = Criteria.POWER_LOW,
            accuracy = Criteria.ACCURACY_FINE,
        )
        configureProvider(
            name = LocationManager.NETWORK_PROVIDER,
            requiresNetwork = true,
            requiresSatellite = false,
            requiresCell = true,
            powerRequirement = Criteria.POWER_MEDIUM,
            accuracy = Criteria.ACCURACY_FINE,
        )

        started = true
    }

    override fun publish(
        point: GeoPoint,
        speedMetersPerSecond: Float,
        bearingDegrees: Float,
    ): Result<Unit> = runCatching {
        check(started) { "mock location provider is not started" }
        require(point.latitude in -90.0..90.0) { "invalid latitude" }
        require(point.longitude in -180.0..180.0) { "invalid longitude" }

        publishToProvider(
            provider = LocationManager.GPS_PROVIDER,
            point = point,
            speedMetersPerSecond = speedMetersPerSecond,
            bearingDegrees = bearingDegrees,
        )
        publishToProvider(
            provider = LocationManager.NETWORK_PROVIDER,
            point = point,
            speedMetersPerSecond = speedMetersPerSecond,
            bearingDegrees = bearingDegrees,
        )
    }

    override fun stop() {
        if (!started) return
        listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER).forEach { provider ->
            runCatching { locationManager.clearTestProviderLocation(provider) }
            runCatching { locationManager.clearTestProviderEnabled(provider) }
            runCatching { locationManager.removeTestProvider(provider) }
        }
        started = false
    }

    private fun configureProvider(
        name: String,
        requiresNetwork: Boolean,
        requiresSatellite: Boolean,
        requiresCell: Boolean,
        powerRequirement: Int,
        accuracy: Int,
    ) {
        runCatching {
            locationManager.addTestProvider(
                name,
                requiresNetwork,
                requiresSatellite,
                requiresCell,
                false,
                true,
                true,
                true,
                powerRequirement,
                accuracy,
            )
        }.onFailure { error ->
            if (error !is IllegalArgumentException) throw error
            // An existing test provider is fine; reuse it.
        }

        locationManager.setTestProviderEnabled(name, true)
    }

    private fun publishToProvider(
        provider: String,
        point: GeoPoint,
        speedMetersPerSecond: Float,
        bearingDegrees: Float,
    ) {
        val location = Location(provider).apply {
            latitude = point.latitude
            longitude = point.longitude
            altitude = 0.0
            accuracy = 3f
            speed = speedMetersPerSecond.coerceAtLeast(0f)
            bearing = ((bearingDegrees % 360f) + 360f) % 360f
            time = System.currentTimeMillis()
            elapsedRealtimeNanos = SystemClock.elapsedRealtimeNanos()
        }
        locationManager.setTestProviderLocation(provider, location)
    }
}
